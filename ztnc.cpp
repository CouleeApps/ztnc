// ztnc
// Copyright (C) 2020 Glenn Smith
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "ZeroTierSockets.h"
#include <cstdio>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>

// Windows "Compatibility"
// YMMV, I can't get MSVC to build libzt and I'm not installing MinGW to test
#ifdef _WIN32
#include <ws2def.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <sys/fcntl.h>

// Well within MTU
#define PACKET_SIZE 1024

//#define _DEBUG
#ifdef _DEBUG
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
bool debug = true;
#else
#define DEBUGF(...)
bool debug = false;
#endif

bool running = true;

void sigint_handler(int) { running = false; }

void echo_server_client(int fd) {
  struct timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;

//  zts_bsd_fcntl(fd, ZTS_F_SETFL, zts_bsd_fcntl(fd, ZTS_F_GETFL, 0) | ZTS_O_NONBLOCK);
  zts_bsd_setsockopt(fd, ZTS_SOL_SOCKET, ZTS_SO_RCVTIMEO, &timeout, sizeof(struct timeval));

  std::thread fd_thread = std::thread([fd]() {
    DEBUGF("Starting fd_thread\n");
    char buf[PACKET_SIZE];
    ssize_t length;
    while (running && (length = zts_bsd_read(fd, buf, PACKET_SIZE)) != 0) {
      if (length < 0) {
        if (zts_errno == ZTS_EAGAIN)
          continue;
        break;
      }
      if (fwrite(buf, 1, length, stdout) < length) {
        perror("fwrite");
        break;
      }
    }
    running = false;
    DEBUGF("Finishing fd_thread\n");
  });

  std::thread stdin_thread = std::thread([fd]() {
    DEBUGF("Starting stdin_thread\n");
    char buf[PACKET_SIZE];
    ssize_t length;

    while (running) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(STDIN_FILENO, &fds);

      struct timeval timeout{};
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;

      int ret = select(1, &fds, nullptr, nullptr, &timeout);
      if (ret == -1) {
        perror("select");
        break;
      }
      if (ret == 0) {
        continue;
      }
      length = read(STDIN_FILENO, buf, PACKET_SIZE);
      if (length <= 0) {
        break;
      }
      buf[length] = 0;
      if (zts_write(fd, buf, length) != length) {
        perror("zts_write");
        break;
      }
    }
    running = false;
    DEBUGF("Finishing stdin_thread\n");
  });

  signal(SIGINT, &sigint_handler);

  DEBUGF("Spawning read threads\n");

  while (running) {
    usleep(100000);
  }

  fd_thread.join();
  stdin_thread.join();

  zts_close(fd);

  DEBUGF("Read threads finished\n");
}

#define TRY(a)                                                                 \
  do {                                                                         \
    if ((a) != 0) {                                                            \
      perror(#a);                                                              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

void print_usage(const char *argv0) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr,
          "    %s [-n <network id>] [-c <cache dir>] <address> <port>\n",
          argv0);
  fprintf(stderr, "    %s [-n <network id>] [-c <cache dir>] -p <port>\n",
          argv0);
}

int main(int argc, char **argv) {
  bool listen = false;
  bool temp_cache = true;
  uint64_t nwid = 0x8056c2e21c000001;
  unsigned short listen_port = 0;
  std::string cache_dir;

  sockaddr_in connect_addr{};

  using namespace boost::program_options;
  try {
    options_description desc{"Options"};
    desc.add_options()
        ("help,h", "Show help")
        ("port,p", value<unsigned short>()->notifier([&](auto value) {
          listen = true;
          listen_port = value;
        }), "Listen on the given port instead of connecting")
        ("cache,c", value<std::string>()->notifier([&](auto value) {
          temp_cache = false;
          cache_dir = value;
        }), "Use the following directory for caching credentials")
        ("network,n", value<std::string>()->notifier([&](auto value) {
          sscanf(value.c_str(), "%llx", &nwid);
        }), "Join this network instead of Earth")
        ;

    command_line_parser parser{argc, argv};
    parser.options(desc);
    parsed_options parsed = parser.run();

    variables_map vm;
    store(parsed, vm);
    notify(vm);

    if (vm.count("help")) {
      print_usage(argv[0]);
      return 0;
    }

    if (!listen) {
      auto extras = collect_unrecognized(parsed.options, include_positional);

      if (extras.size() < 2) {
        print_usage(argv[0]);
        return -1;
      }

      inet_pton(AF_INET, extras[0].c_str(), &connect_addr.sin_addr);
      connect_addr.sin_family = AF_INET;
      connect_addr.sin_port = htons(atoi(extras[1].c_str()));
    }
  } catch (const error &ex) {
    std::cerr << ex.what() << std::endl;
    return -1;
  }

  if (temp_cache) {
    cache_dir = boost::filesystem::unique_path().string();
    boost::filesystem::create_directories(cache_dir);
  }

  // Initialize node
  TRY(zts_init_from_storage(cache_dir.c_str()));

  // Start node
  TRY(zts_node_start());

  fprintf(stderr, "Connecting...\n");

  DEBUGF("Waiting for node to come online\n");
  while (! zts_node_is_online()) {
    zts_util_delay(50);
  }
  DEBUGF("Public identity (node ID) is %llx\n", zts_node_get_id());

  // Join network

  fprintf(stderr, "Joining network...\n");
  DEBUGF("Joining network %llx\n", nwid);
  if (zts_net_join(nwid) != ZTS_ERR_OK) {
    DEBUGF("Unable to join network. Exiting.\n");
    exit(1);
  }
  DEBUGF("Don't forget to authorize this device in my.zerotier.com or the web API!\n");
  DEBUGF("Waiting for join to complete\n");
  while (!zts_net_transport_is_ready(nwid)) {
    zts_util_delay(50);
  }

  fprintf(stderr, "Getting ip address...\n");

  DEBUGF("Waiting for address assignment from network\n");
  while (!zts_addr_is_assigned(nwid, ZTS_AF_INET)) {
    zts_util_delay(50);
  }
  char ipstr[ZTS_IP_MAX_STR_LEN] = { 0 };
  zts_addr_get_str(nwid, ZTS_AF_INET, ipstr, ZTS_IP_MAX_STR_LEN);
  DEBUGF("IP address on network %llx is %s\n", nwid, ipstr);

  DEBUGF("Node ID: %16llx\n", my_id);
  DEBUGF("Address: %s\n", inet_ntoa(my_addr->sin_addr));
  DEBUGF("Status: %x\n", zts_get_node_status());

  int fd = zts_bsd_socket(ZTS_AF_INET, ZTS_SOCK_STREAM, 0);

  if (listen) {
    // Tell the user where they are listening
    fprintf(stderr, "Listening on %s:%hu\n", ipstr, listen_port);

    // Listen on any addr (on zt)
    zts_sockaddr_in listen_addr;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_family = AF_INET;

    zts_bsd_bind(fd, (const zts_sockaddr *)&listen_addr, sizeof(sockaddr_in));
    zts_listen(fd, 10);

    // Wait for a client to connect
    zts_sockaddr_in client_addr;
    socklen_t client_len;
    int client_fd = zts_bsd_accept(fd, (zts_sockaddr *)&client_addr, &client_len);
    fprintf(stderr, "Established Connection\n");
    echo_server_client(client_fd);
  } else {
    zts_bsd_connect(fd, (const zts_sockaddr *)&connect_addr, sizeof(connect_addr));
    fprintf(stderr, "Established Connection\n");
    echo_server_client(fd);
  }

  // Disconnect like a good peer
  fprintf(stderr, "Disconnecting...\n");
  TRY(zts_node_stop());

  if (temp_cache) {
    boost::filesystem::remove_all(cache_dir);
  }

  fprintf(stderr, "Connection terminated\n");
  return 0;
}