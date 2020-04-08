
#include "ZeroTier.h"
#include <cstdio>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <ws2def.h>
#else
#include <arpa/inet.h>
#endif

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

// Well within MTU
#define PACKET_SIZE 1024

// Message queue class with *futures*
template <typename MsgType> class MessageList {
public:
  typedef std::function<bool(MsgType &)> test_fn_t;

  // Await a message that satisfies the provided function
  // Returns a future whose value will be the first message that passed the
  // function
  std::future<MsgType> expect(test_fn_t fn) {
    auto m = std::make_unique<Message>(std::move(fn));

    pending_list_mutex.lock();
    pending_messages.push_back(std::move(m));
    auto &message = pending_messages.back();
    auto fut = message->fut.get_future();
    pending_list_mutex.unlock();

    return fut;
  }

  // Post receive of a message, testing against all pending expects
  // Returns the number of expects that passed their test
  int on_message(MsgType msg) {
    int count = 0;
    pending_list_mutex.lock();
    for (auto it = pending_messages.begin(); it != pending_messages.end();) {
      if ((*it)->test_fn(msg)) {
        count += 1;
        (*it)->fut.set_value(msg);
        it = pending_messages.erase(it);
      } else {
        ++it;
      }
    }
    pending_list_mutex.unlock();
    return count;
  }

private:
  struct Message {
    std::promise<MsgType> fut;
    test_fn_t test_fn;

    explicit Message(test_fn_t &&fn) : test_fn(std::move(fn)) {}
  };

  std::recursive_mutex pending_list_mutex;
  std::vector<std::unique_ptr<Message>> pending_messages;
};

// Wrapper class for libzt's structure type that we don't have ownership of
// Copies a message into an internal reference and frees it on destruct
class ZtsCallbackMsg {
  zts_callback_msg *internal;

  struct zts_callback_msg *copy_msg(zts_callback_msg *orig) {
    zts_callback_msg *copy = new zts_callback_msg;
    copy->eventCode = orig->eventCode;
    if (orig->node) {
      copy->node = new zts_node_details;
      *copy->node = *orig->node;
    } else {
      copy->node = nullptr;
    }
    if (orig->network) {
      copy->network = new zts_network_details;
      *copy->network = *orig->network;
    } else {
      copy->network = nullptr;
    }
    if (orig->netif) {
      copy->netif = new zts_netif_details;
      *copy->netif = *orig->netif;
    } else {
      copy->netif = nullptr;
    }
    if (orig->route) {
      copy->route = new zts_virtual_network_route;
      *copy->route = *orig->route;
    } else {
      copy->route = nullptr;
    }
    if (orig->path) {
      copy->path = new zts_physical_path;
      *copy->path = *orig->path;
    } else {
      copy->path = nullptr;
    }
    if (orig->peer) {
      copy->peer = new zts_peer_details;
      *copy->peer = *orig->peer;
    } else {
      copy->peer = nullptr;
    }
    if (orig->addr) {
      copy->addr = new zts_addr_details;
      *copy->addr = *orig->addr;
    } else {
      copy->addr = nullptr;
    }
    return copy;
  }

public:
  explicit ZtsCallbackMsg(zts_callback_msg *msg) { internal = copy_msg(msg); }

  ~ZtsCallbackMsg() {
    if (internal->node != nullptr) {
      delete internal->node;
      internal->node = nullptr;
    }
    if (internal->network != nullptr) {
      delete internal->network;
      internal->network = nullptr;
    }
    if (internal->netif != nullptr) {
      delete internal->netif;
      internal->netif = nullptr;
    }
    if (internal->route != nullptr) {
      delete internal->route;
      internal->route = nullptr;
    }
    if (internal->path != nullptr) {
      delete internal->path;
      internal->path = nullptr;
    }
    if (internal->peer != nullptr) {
      delete internal->peer;
      internal->peer = nullptr;
    }
    if (internal->addr != nullptr) {
      delete internal->addr;
      internal->addr = nullptr;
    }
    delete internal;
  }

  zts_callback_msg *operator->() const { return internal; }
};

MessageList<std::shared_ptr<ZtsCallbackMsg>> list{};

#ifdef _DEBUG
#define DEBUGF printf
bool debug = true;
#else
#define DEBUGF(...)
bool debug = false;
#endif

void event_callback(struct zts_callback_msg *msg) {
  int received = list.on_message(std::make_shared<ZtsCallbackMsg>(msg));

  switch (msg->eventCode) {
  case ZTS_EVENT_NODE_OFFLINE:
    DEBUGF("ZTS_EVENT_NODE_OFFLINE\n");
    break;
  case ZTS_EVENT_NODE_ONLINE:
    DEBUGF("ZTS_EVENT_NODE_ONLINE\n");
    break;
  case ZTS_EVENT_NODE_DOWN:
    DEBUGF("ZTS_EVENT_NODE_DOWN\n");
    break;
  case ZTS_EVENT_NODE_IDENTITY_COLLISION:
    DEBUGF("ZTS_EVENT_NODE_IDENTITY_COLLISION\n");
    break;
  case ZTS_EVENT_NODE_UNRECOVERABLE_ERROR:
    DEBUGF("ZTS_EVENT_NODE_UNRECOVERABLE_ERROR\n");
    break;
  case ZTS_EVENT_NODE_NORMAL_TERMINATION:
    DEBUGF("ZTS_EVENT_NODE_NORMAL_TERMINATION\n");
    break;
  case ZTS_EVENT_NETWORK_NOT_FOUND:
    DEBUGF("ZTS_EVENT_NETWORK_NOT_FOUND\n");
    break;
  case ZTS_EVENT_NETWORK_CLIENT_TOO_OLD:
    DEBUGF("ZTS_EVENT_NETWORK_CLIENT_TOO_OLD\n");
    break;
  case ZTS_EVENT_NETWORK_REQUESTING_CONFIG:
    DEBUGF("ZTS_EVENT_NETWORK_REQUESTING_CONFIG\n");
    break;
  case ZTS_EVENT_NETWORK_OK:
    DEBUGF("ZTS_EVENT_NETWORK_OK\n");
    break;
  case ZTS_EVENT_NETWORK_ACCESS_DENIED:
    DEBUGF("ZTS_EVENT_NETWORK_ACCESS_DENIED\n");
    break;
  case ZTS_EVENT_NETWORK_READY_IP4:
    DEBUGF("ZTS_EVENT_NETWORK_READY_IP4\n");
    break;
  case ZTS_EVENT_NETWORK_READY_IP6:
    DEBUGF("ZTS_EVENT_NETWORK_READY_IP6\n");
    break;
  case ZTS_EVENT_NETWORK_READY_IP4_IP6:
    DEBUGF("ZTS_EVENT_NETWORK_READY_IP4_IP6\n");
    break;
  case ZTS_EVENT_NETWORK_DOWN:
    DEBUGF("ZTS_EVENT_NETWORK_DOWN\n");
    break;
  case ZTS_EVENT_STACK_UP:
    DEBUGF("ZTS_EVENT_STACK_UP\n");
    break;
  case ZTS_EVENT_STACK_DOWN:
    DEBUGF("ZTS_EVENT_STACK_DOWN\n");
    break;
  case ZTS_EVENT_NETIF_UP:
    DEBUGF("ZTS_EVENT_NETIF_UP\n");
    break;
  case ZTS_EVENT_NETIF_DOWN:
    DEBUGF("ZTS_EVENT_NETIF_DOWN\n");
    break;
  case ZTS_EVENT_NETIF_REMOVED:
    DEBUGF("ZTS_EVENT_NETIF_REMOVED\n");
    break;
  case ZTS_EVENT_NETIF_LINK_UP:
    DEBUGF("ZTS_EVENT_NETIF_LINK_UP\n");
    break;
  case ZTS_EVENT_NETIF_LINK_DOWN:
    DEBUGF("ZTS_EVENT_NETIF_LINK_DOWN\n");
    break;
  case ZTS_EVENT_PEER_P2P:
    DEBUGF("ZTS_EVENT_PEER_P2P\n");
    break;
  case ZTS_EVENT_PEER_RELAY:
    DEBUGF("ZTS_EVENT_PEER_RELAY\n");
    break;
  case ZTS_EVENT_PEER_UNREACHABLE:
    DEBUGF("ZTS_EVENT_PEER_UNREACHABLE\n");
    break;
  case ZTS_EVENT_PATH_DISCOVERED:
    DEBUGF("ZTS_EVENT_PATH_DISCOVERED\n");
    break;
  case ZTS_EVENT_PATH_ALIVE:
    DEBUGF("ZTS_EVENT_PATH_ALIVE\n");
    break;
  case ZTS_EVENT_PATH_DEAD:
    DEBUGF("ZTS_EVENT_PATH_DEAD\n");
    break;
  case ZTS_EVENT_ROUTE_ADDED:
    DEBUGF("ZTS_EVENT_ROUTE_ADDED\n");
    break;
  case ZTS_EVENT_ROUTE_REMOVED:
    DEBUGF("ZTS_EVENT_ROUTE_REMOVED\n");
    break;
  case ZTS_EVENT_ADDR_ADDED_IP4:
    DEBUGF("ZTS_EVENT_ADDR_ADDED_IP4\n");
    break;
  case ZTS_EVENT_ADDR_REMOVED_IP4:
    DEBUGF("ZTS_EVENT_ADDR_REMOVED_IP4\n");
    break;
  case ZTS_EVENT_ADDR_ADDED_IP6:
    DEBUGF("ZTS_EVENT_ADDR_ADDED_IP6\n");
    break;
  case ZTS_EVENT_ADDR_REMOVED_IP6:
    DEBUGF("ZTS_EVENT_ADDR_REMOVED_IP6\n");
    break;
  }
  if (received > 0) {
    DEBUGF("Processed by %d callbacks\n", received);
  }
}

MessageList<bool> threadList;

void sigint_handler(int) { threadList.on_message(true); }

void echo_server_client(int fd) {
  auto awaiting = threadList.expect([](bool) { return true; });

  std::thread([fd]() {
    DEBUGF("Starting fd_thread\n");
    char buf[PACKET_SIZE];
    ssize_t length;
    while ((length = zts_read(fd, buf, PACKET_SIZE)) > 0) {
      fwrite(buf, 1, length, stdout);
    }
    zts_close(fd);
    threadList.on_message(true);
    DEBUGF("Finishing fd_thread\n");
  }).detach();

  std::thread([fd]() {
    DEBUGF("Starting stdin_thread\n");
    char buf[PACKET_SIZE];
    ssize_t length;

    while ((length = fread(buf, 1, PACKET_SIZE, stdin)) > 0) {
      buf[length] = 0;
      if (zts_write(fd, buf, length) != length) {
        perror("zts_write");
        return;
      }
    }
    zts_close(fd);
    threadList.on_message(true);
    DEBUGF("Finishing stdin_thread\n");
  }).detach();

  signal(SIGINT, &sigint_handler);

  DEBUGF("Spawning read threads\n");
  awaiting.wait();
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
  fprintf(stderr, "    %s [-n <network id>] [-c <cache dir>] -l <port>\n",
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

  // Start futures before we connect so they can resolve
  auto wait_online = list.expect([](std::shared_ptr<ZtsCallbackMsg> &msg) {
    return (*msg)->eventCode == ZTS_EVENT_NODE_ONLINE;
  });
  auto wait_ip4 = list.expect([nwid](std::shared_ptr<ZtsCallbackMsg> &msg) {
    return (*msg)->eventCode == ZTS_EVENT_ADDR_ADDED_IP4 &&
           (*msg)->addr->nwid == nwid;
  });
  auto wait_ip4_ready = list.expect([](std::shared_ptr<ZtsCallbackMsg> &msg) {
    return (*msg)->eventCode == ZTS_EVENT_NETWORK_READY_IP4;
  });
  auto wait_disconnect = list.expect([](std::shared_ptr<ZtsCallbackMsg> &msg) {
    return (*msg)->eventCode == ZTS_EVENT_NODE_DOWN;
  });

  fprintf(stderr, "Connecting to ZeroTier...\n");

  TRY(zts_start(cache_dir.c_str(), &event_callback, 9994));
  wait_online.wait();

  fprintf(stderr, "Joining Network...\n");
  TRY(zts_join(nwid));

  // Wait until we have an ipv4
  wait_ip4.wait();

  // Get our ipv4 from the network
  auto v = wait_ip4.get();
  sockaddr_in *my_addr = (sockaddr_in *)&(*v)->addr->addr;
  uint64_t my_id = zts_get_node_id();

  DEBUGF("Node ID: %16llx\n", my_id);
  DEBUGF("Address: %s\n", inet_ntoa(my_addr->sin_addr));
  DEBUGF("Status: %x\n", zts_get_node_status());

  // Wait for network ready
  fprintf(stderr, "Waiting for Network...\n");
  wait_ip4_ready.wait();

  int fd = zts_socket(ZTS_AF_INET, ZTS_SOCK_STREAM, 0);

  if (listen) {
    // Tell the user where they are listening
    fprintf(stderr, "Listening at (%llx) %s:%d\n", nwid,
            inet_ntoa(my_addr->sin_addr), listen_port);

    // Listen on any addr (on zt)
    sockaddr_in listen_addr;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_family = AF_INET;

    zts_bind(fd, (sockaddr *)&listen_addr, sizeof(sockaddr_in));
    zts_listen(fd, 10);

    // Wait for a client to connect
    sockaddr_in client_addr;
    socklen_t client_len;
    int client_fd = zts_accept(fd, (sockaddr *)&client_addr, &client_len);
    fprintf(stderr, "Established Connection\n");
    echo_server_client(client_fd);
  } else {
    zts_connect(fd, (const sockaddr *)&connect_addr, sizeof(connect_addr));
    fprintf(stderr, "Established Connection\n");
    echo_server_client(fd);
  }

  // Disconnect like a good peer
  fprintf(stderr, "Disconnecting...\n");
  TRY(zts_stop());
  wait_disconnect.wait();

  if (temp_cache) {
    boost::filesystem::remove_all(cache_dir);
  }

  fprintf(stderr, "Connection terminated\n");
  return 0;
}