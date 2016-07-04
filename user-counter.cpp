#include <iostream>
#include <chrono>
#include <list>
#include <map>

extern "C" {
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <unistd.h>
}

typedef std::array<unsigned char, 16> userid_t;

class user {
public:
	userid_t id;
	std::chrono::time_point<std::chrono::steady_clock> time;
};

static void show_usage() {
	std::cerr << "Usage: user-counter <listen-address>" << std::endl;
}

int main(int const argc, char const* const argv[]) {
	if (argc != 2) {
		show_usage();
		return EXIT_FAILURE;
	}

	std::list<user> users;
	std::map<userid_t, decltype(users)::const_iterator> user_map;
	auto const period = std::chrono::minutes(15);

	int const s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1) {
		perror("Failed to create socket");
		return EXIT_FAILURE;
	}

	{
		sockaddr_in listen_address;
		listen_address.sin_family = AF_INET;
		listen_address.sin_port = htons(30051);

		if (inet_pton(AF_INET, argv[1], &listen_address.sin_addr) != 1) {
			show_usage();
			return EXIT_FAILURE;
		}

		if (bind(s, reinterpret_cast<struct sockaddr*>(&listen_address), sizeof listen_address) == -1) {
			perror("Failed to bind to socket");
			return EXIT_FAILURE;
		}
	}

	for (;;) {
		struct sockaddr_in reply_address;
		socklen_t reply_address_size = sizeof reply_address;

		user new_user;
		ssize_t const r = recvfrom(
			s, new_user.id.data(), new_user.id.size(), 0,
			reinterpret_cast<struct sockaddr*>(&reply_address), &reply_address_size
		);

		if (r == -1) {
			perror("Failed to read from socket");
			break;
		}

		if (r != new_user.id.size()) {
			std::cerr << "Ignoring message of size " << r << std::endl;
			continue;
		}

		auto const now = std::chrono::steady_clock::now();
		auto const cutoff = now - period;

		new_user.time = now;

		auto const existing = user_map.find(new_user.id);

		if (existing != user_map.cend()) {
			users.erase(existing->second);
		}

		users.push_front(new_user);
		user_map[new_user.id] = users.cbegin();

		for (user back; !users.empty() && (back = users.back()).time < cutoff;) {
			user_map.erase(back.id);
			users.pop_back();
		}

		size_t const user_count = users.size();
		uint8_t const reply[4] = {
			static_cast<uint8_t>(user_count >> 24),
			static_cast<uint8_t>(user_count >> 16),
			static_cast<uint8_t>(user_count >> 8),
			static_cast<uint8_t>(user_count),
		};

		if (sendto(s, reply, sizeof reply, 0, reinterpret_cast<struct sockaddr*>(&reply_address), sizeof reply_address) == -1) {
			perror("Failed to send reply");
		}
	}

	close(s);
	return EXIT_FAILURE;
}
