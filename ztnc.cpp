
#include <stdio.h>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <mutex>
#include <future>

#include "ZeroTier.h"

class MessageList {
public:
	typedef std::function<bool(struct zts_callback_msg *)> test_fn_t;
	std::future<struct zts_callback_msg *> expect(test_fn_t fn) {
		auto m = std::make_unique<Message>(std::move(fn));

		pending_list_mutex.lock();
		pending_messages.push_back(std::move(m));
		auto &message = pending_messages.back();
		pending_list_mutex.unlock();

		return message->fut.get_future();
	}

	int on_message(struct zts_callback_msg *msg) {
		int count = 0;
		pending_list_mutex.lock();
		for (auto it = pending_messages.begin(); it != pending_messages.end(); ) {
			if ((*it)->test_fn(msg)) {
				count += 1;
				(*it)->fut.set_value(copy_msg(msg));
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
		std::promise<struct zts_callback_msg *> fut;
		test_fn_t test_fn;

		explicit Message(test_fn_t &&fn) : test_fn(std::move(fn)) {}
	};

	static struct zts_callback_msg *copy_msg(struct zts_callback_msg *orig) {
		struct zts_callback_msg *copy = new struct zts_callback_msg;
		copy->eventCode = orig->eventCode;
		if (orig->node) {
			copy->node = new struct zts_node_details; *copy->node = *orig->node;
		} else {
			copy->node = nullptr;
		}
		if (orig->network) {
			copy->network = new struct zts_network_details; *copy->network = *orig->network;
		} else {
			copy->network = nullptr;
		}
		if (orig->netif) {
			copy->netif = new struct zts_netif_details; *copy->netif = *orig->netif;
		} else {
			copy->netif = nullptr;
		}
		if (orig->route) {
			copy->route = new struct zts_virtual_network_route; *copy->route = *orig->route;
		} else {
			copy->route = nullptr;
		}
		if (orig->path) {
			copy->path = new struct zts_physical_path; *copy->path = *orig->path;
		} else {
			copy->path = nullptr;
		}
		if (orig->peer) {
			copy->peer = new struct zts_peer_details; *copy->peer = *orig->peer;
		} else {
			copy->peer = nullptr;
		}
		if (orig->addr) {
			copy->addr = new struct zts_addr_details; *copy->addr = *orig->addr;
		} else {
			copy->addr = nullptr;
		}
		return copy;
	}

	std::recursive_mutex pending_list_mutex;
	std::vector<std::unique_ptr<Message>> pending_messages;
};

MessageList list{};
#ifdef _DEBUG
#define DEBUGF printf
bool debug = true;
#else
#define DEBUGF(...)
bool debug = false;
#endif

void myZeroTierEventCallback(struct zts_callback_msg *msg)
{
	int received = list.on_message(msg);

	switch (msg->eventCode)
	{
		case ZTS_EVENT_NODE_OFFLINE: DEBUGF("ZTS_EVENT_NODE_OFFLINE\n"); break;
		case ZTS_EVENT_NODE_ONLINE: DEBUGF("ZTS_EVENT_NODE_ONLINE\n"); break;
		case ZTS_EVENT_NODE_DOWN: DEBUGF("ZTS_EVENT_NODE_DOWN\n"); break;
		case ZTS_EVENT_NODE_IDENTITY_COLLISION: DEBUGF("ZTS_EVENT_NODE_IDENTITY_COLLISION\n"); break;
		case ZTS_EVENT_NODE_UNRECOVERABLE_ERROR: DEBUGF("ZTS_EVENT_NODE_UNRECOVERABLE_ERROR\n"); break;
		case ZTS_EVENT_NODE_NORMAL_TERMINATION: DEBUGF("ZTS_EVENT_NODE_NORMAL_TERMINATION\n"); break;
		case ZTS_EVENT_NETWORK_NOT_FOUND: DEBUGF("ZTS_EVENT_NETWORK_NOT_FOUND\n"); break;
		case ZTS_EVENT_NETWORK_CLIENT_TOO_OLD: DEBUGF("ZTS_EVENT_NETWORK_CLIENT_TOO_OLD\n"); break;
		case ZTS_EVENT_NETWORK_REQUESTING_CONFIG: DEBUGF("ZTS_EVENT_NETWORK_REQUESTING_CONFIG\n"); break;
		case ZTS_EVENT_NETWORK_OK: DEBUGF("ZTS_EVENT_NETWORK_OK\n"); break;
		case ZTS_EVENT_NETWORK_ACCESS_DENIED: DEBUGF("ZTS_EVENT_NETWORK_ACCESS_DENIED\n"); break;
		case ZTS_EVENT_NETWORK_READY_IP4: DEBUGF("ZTS_EVENT_NETWORK_READY_IP4\n"); break;
		case ZTS_EVENT_NETWORK_READY_IP6: DEBUGF("ZTS_EVENT_NETWORK_READY_IP6\n"); break;
		case ZTS_EVENT_NETWORK_READY_IP4_IP6: DEBUGF("ZTS_EVENT_NETWORK_READY_IP4_IP6\n"); break;
		case ZTS_EVENT_NETWORK_DOWN: DEBUGF("ZTS_EVENT_NETWORK_DOWN\n"); break;
		case ZTS_EVENT_STACK_UP: DEBUGF("ZTS_EVENT_STACK_UP\n"); break;
		case ZTS_EVENT_STACK_DOWN: DEBUGF("ZTS_EVENT_STACK_DOWN\n"); break;
		case ZTS_EVENT_NETIF_UP: DEBUGF("ZTS_EVENT_NETIF_UP\n"); break;
		case ZTS_EVENT_NETIF_DOWN: DEBUGF("ZTS_EVENT_NETIF_DOWN\n"); break;
		case ZTS_EVENT_NETIF_REMOVED: DEBUGF("ZTS_EVENT_NETIF_REMOVED\n"); break;
		case ZTS_EVENT_NETIF_LINK_UP: DEBUGF("ZTS_EVENT_NETIF_LINK_UP\n"); break;
		case ZTS_EVENT_NETIF_LINK_DOWN: DEBUGF("ZTS_EVENT_NETIF_LINK_DOWN\n"); break;
		case ZTS_EVENT_PEER_P2P: DEBUGF("ZTS_EVENT_PEER_P2P\n"); break;
		case ZTS_EVENT_PEER_RELAY: DEBUGF("ZTS_EVENT_PEER_RELAY\n"); break;
		case ZTS_EVENT_PEER_UNREACHABLE: DEBUGF("ZTS_EVENT_PEER_UNREACHABLE\n"); break;
		case ZTS_EVENT_PATH_DISCOVERED: DEBUGF("ZTS_EVENT_PATH_DISCOVERED\n"); break;
		case ZTS_EVENT_PATH_ALIVE: DEBUGF("ZTS_EVENT_PATH_ALIVE\n"); break;
		case ZTS_EVENT_PATH_DEAD: DEBUGF("ZTS_EVENT_PATH_DEAD\n"); break;
		case ZTS_EVENT_ROUTE_ADDED: DEBUGF("ZTS_EVENT_ROUTE_ADDED\n"); break;
		case ZTS_EVENT_ROUTE_REMOVED: DEBUGF("ZTS_EVENT_ROUTE_REMOVED\n"); break;
		case ZTS_EVENT_ADDR_ADDED_IP4: DEBUGF("ZTS_EVENT_ADDR_ADDED_IP4\n"); break;
		case ZTS_EVENT_ADDR_REMOVED_IP4: DEBUGF("ZTS_EVENT_ADDR_REMOVED_IP4\n"); break;
		case ZTS_EVENT_ADDR_ADDED_IP6: DEBUGF("ZTS_EVENT_ADDR_ADDED_IP6\n"); break;
		case ZTS_EVENT_ADDR_REMOVED_IP6: DEBUGF("ZTS_EVENT_ADDR_REMOVED_IP6\n"); break;
	}
	if (received > 0) {
		DEBUGF("Processed by %d callbacks\n", received);
	}
}

std::promise<void> finished;

void sigint_handler(int) {
	finished.set_value();
}

void echo_server_client(int fd) {
	auto fut = finished.get_future();
	std::thread([fd]() {
		DEBUGF("Starting fd_thread\n");
		char buf[2048];
		size_t length;
		while ((length = zts_read(fd, buf, 2048 - 1)) != 0) {
			buf[length] = 0;
			printf("%s", buf);
		}
		zts_close(fd);
		finished.set_value();
		DEBUGF("Finishing fd_thread\n");
	}).detach();

	std::thread([fd]() {
		DEBUGF("Starting stdin_thread\n");
		char buf[2048];
		size_t length;
		while ((length = read(STDIN_FILENO, buf, 2048 - 1)) != 0) {
			buf[length] = 0;
			if (zts_write(fd, buf, length) != length) {
				perror("zts_write");
				return;
			}
		}
		zts_close(fd);
		finished.set_value();
		DEBUGF("Finishing stdin_thread\n");
	}).detach();

	signal(SIGINT, &sigint_handler);

	DEBUGF("Spawning read threads\n");
	fut.wait();
	DEBUGF("Read threads finished\n");
}

#define TRY(a) do { if ((a) != 0) { perror(#a); return 1; } } while (0)

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "    %s <network id> <address> <port>\n", argv[0]);
		fprintf(stderr, "    %s <network id> -l <port>\n", argv[0]);
	}

	uint64_t nwid = 0x8056c2e21c000001;
	sscanf(argv[1], "%llx", &nwid);

	bool listen = false;
	if (strcmp(argv[2], "-l") == 0) {
		listen = true;
	}

	auto wait_online = list.expect([](struct zts_callback_msg *msg) { return msg->eventCode == ZTS_EVENT_NODE_ONLINE; });
	auto wait_ip4 = list.expect([nwid](struct zts_callback_msg *msg) { return msg->eventCode == ZTS_EVENT_ADDR_ADDED_IP4 && msg->addr->nwid == nwid; });
	auto wait_ip4_ready = list.expect([](struct zts_callback_msg *msg) { return msg->eventCode == ZTS_EVENT_NETWORK_READY_IP4; });

	fprintf(stderr, "Connecting to ZeroTier...\n");

	TRY(zts_start("./config", &myZeroTierEventCallback, 9994));
	wait_online.wait();

	fprintf(stderr, "Joining Network...\n");

	TRY(zts_join(nwid));
	wait_ip4.wait();
	auto v = wait_ip4.get();

	struct sockaddr_in *my_addr = (struct sockaddr_in *)&v->addr->addr;
	uint64_t my_id = zts_get_node_id();

	DEBUGF("Node ID: %16llx\n", my_id);
	DEBUGF("Address: %s\n", inet_ntoa(my_addr->sin_addr));
	DEBUGF("Status: %x\n", zts_get_node_status());

	// Wait for network ready
	fprintf(stderr, "Waiting for Network...\n");
	wait_ip4_ready.wait();

	int fd = zts_socket(ZTS_AF_INET, ZTS_SOCK_STREAM, 0);

	if (listen) {
		unsigned short port = atoi(argv[3]);
		fprintf(stderr, "Listening at %s:%d\n", inet_ntoa(my_addr->sin_addr), port);

		struct sockaddr_in listen_addr;
		listen_addr.sin_port = htons(port);
		listen_addr.sin_addr.s_addr = INADDR_ANY;
		listen_addr.sin_family = AF_INET;

		zts_bind(fd, (struct sockaddr *)&listen_addr, sizeof(struct sockaddr_in));
		zts_listen(fd, 10);

		struct sockaddr_in client_addr;
		socklen_t client_len;

		int client_fd = zts_accept(fd, (struct sockaddr *)&client_addr, &client_len);
		fprintf(stderr, "Established Connection\n");
		echo_server_client(client_fd);
	} else {
		struct sockaddr_in addr{};
		inet_pton(AF_INET, argv[2], &addr.sin_addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);

		zts_connect(fd, (const struct sockaddr *) &addr, sizeof(addr));
		fprintf(stderr, "Established Connection\n");
		echo_server_client(fd);
	}
	TRY(zts_stop());

	list.expect([](struct zts_callback_msg *msg) { return msg->eventCode == ZTS_EVENT_NODE_DOWN; });

	exit(0);
}