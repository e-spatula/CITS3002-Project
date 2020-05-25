#include "tcp.h"

void handle_tcp(Node& this_node, string& message, int socket) {
	std::regex reg_str("to=\\w+");
	std::smatch match; 
	std::regex_search(message, match, reg_str);
	string destination = match.str();
	if(match.size() == 0) {
		list<string> msgs;
		msgs.push_back("Ouch! bad request");
		string response = http_string(400, "Bad request", msgs);
		if(send(socket, response.c_str(), response.size(), 0) < 0) {
			cout << "Failed to respond to socket, exiting" << endl;
			this_node.quit(1);
		}
		this_node.remove_socket(socket);
	}
	size_t pos = destination.find("=");
	// Trim off prefix
	destination.erase(0, pos + 1);
	
	Frame request_frame = Frame(
		this_node.name,
		destination,
		list<string>(),
		this_node.seqno,
		-1,
		REQUEST
	);
	this_node.response_sockets[this_node.seqno] = socket;
	this_node.seqno = (this_node.seqno + 1) % MAX_INT;
	this_node.check_timetable();
	// send_frame_to_neighbours(this_node, request_frame);
}