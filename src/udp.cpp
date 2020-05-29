/*
Utility functions for handling UDP packets across the network.
*/
#include "udp.h"

/*
Processes request frames.

Request frames are either responded to immediately if they are destined for the
current node, forwarded to neighbours if they haven't been seen before, or negatively
responded to if they have been seen before.

Arguments:
  this_node: Node object representing a station on the network
  in_frame: Frame object representing inbound frame
Returns: 
  void
*/
void process_request_frame(Node &this_node, Frame &in_frame) {
  cout << this_node.name << " received request " << in_frame.to_string()
       << endl;
  cout << "from " << in_frame.src.back() << endl;

  uint16_t out_port = this_node.get_port_from_name(in_frame.src.back());
  if (out_port < 0) {
    cout << "Failed to find " << in_frame.src.back() << " in neighbours list"
         << endl;
    this_node.quit(1);
  }
  if (in_frame.dest == this_node.name) {
    // You've got mail!
    cout << this_node.name << " received a frame for itself, responding"
         << endl;
    in_frame.src.push_back(this_node.name);
    Frame response_frame = Frame(this_node.name, in_frame.origin, in_frame.src,
                                 in_frame.seqno, in_frame.time, RESPONSE);
    string response_str = response_frame.to_string();
    this_node.send_udp(out_port, response_str);

  } else if (std::find(in_frame.src.begin(), in_frame.src.end(),
                       this_node.name) == in_frame.src.end()) {
    send_frame_to_neighbours(this_node, in_frame);
  } else {
    // We have a cycle
    cout << this_node.name << " squashing cycle" << endl;
    in_frame.src.push_back(this_node.name);
    Frame response_frame = Frame(in_frame.dest, in_frame.origin, in_frame.src,
                                 in_frame.seqno, -1, RESPONSE);
    string response_str = response_frame.to_string();
    this_node.send_udp(out_port, response_str);
  }
}

/*
Processes response frames.

Searches through the list of response objects a node has stored to find the 
frame the response frame pertains to, adjusts its fastest time if a faster route
has been found, and decrements its count of responses remaining. If no more responses
are expected a response frame is sent to the sender, or if the sender was the current
node a response is sent to the browser.

Arguments:
  this_node: Node object representing a station on the network
  in_frame: Frame object representing inbound frame
Returns: 
  void
*/
void process_response_frame(Node &this_node, Frame &in_frame) {
  cout << this_node.name << " received response " << in_frame.to_string()
       << " from " << in_frame.src.back() << endl;
  // Remember who sent us the response
  string src_node = in_frame.src.back();
  in_frame.src.pop_back();
  // Take ourselves out of the src
  // in_frame.src.pop_back();
  cout << this_node.name << " frame " << in_frame.to_string() << endl; 
  Response *resp_obj = this_node.find_response_obj(
      in_frame.dest, in_frame.seqno, in_frame.src);
  if (resp_obj == NULL) {
    cout << "Couldn't find response object for frame " << in_frame.to_string()
         << endl;
    this_node.quit(1);
  }
  if (resp_obj->time == -1 && in_frame.time > 0) {
    // Anything is faster than not getting there at all
    resp_obj->time = in_frame.time;
    resp_obj->stop = src_node;
  } else if (in_frame.time < resp_obj->time && in_frame.time > 0) {
    // We've found a faster route
    resp_obj->time = in_frame.time;
    resp_obj->stop = src_node;
  }
  cout << this_node.name << " remaining responses for " << in_frame.to_string() << endl << resp_obj->remaining_responses << endl;
  (resp_obj->remaining_responses) = (resp_obj->remaining_responses) - 1;
  cout << this_node.name << " remaining responses for " << in_frame.to_string() << endl << resp_obj->remaining_responses << endl; 
  if ((resp_obj->remaining_responses) == 0) {
    if (in_frame.dest == this_node.name) {
      cout << "End of the line, respond to TCP socket" << endl;
      ostringstream arrival_time;
      ostringstream itinerary;
      arrival_time << "Arrival Time: ";
      itinerary << "Next leg of trip: ";
      if (resp_obj->time < 0) {
        // We couldn't get there
        arrival_time << "couldn't get there";
        itinerary << "None";
      } else {
        int start_time = current_time();
        // int start_time = 500;
        string next_journey =
            this_node.find_itinerary(resp_obj->stop, start_time);
        if (next_journey.size() == 0) {
          cout << "Couldn't find itinerary to return, exiting" << endl;
          this_node.quit(1);
        }
        int hours = resp_obj->time / 60;
        int minutes = resp_obj->time % 60;
        arrival_time << hours << ":";
        if (minutes < 10) {
          arrival_time << "0" << minutes;
        } else {
          arrival_time << minutes;
        }
        itinerary << next_journey;
      }
      list<string> http_lines({arrival_time.str(), itinerary.str()});
      string http_response = http_string(200, "OK", http_lines);
      int out_socket = this_node.response_sockets[resp_obj->seqno];
      this_node.send_tcp(out_socket, http_response);
      this_node.remove_socket(out_socket);
    } else {
      // Find out who sent the frame to us and then add ourselves back in
      in_frame.src.pop_back();
      uint16_t out_port = this_node.get_port_from_name(in_frame.src.back());
      in_frame.src.push_back(this_node.name);
      
      Frame response_frame =
          Frame(in_frame.origin, resp_obj->origin, in_frame.src,
                resp_obj->seqno, resp_obj->time, RESPONSE);
      string response_str = response_frame.to_string();
      cout << this_node.name << " sending response frame " << response_str << " to " << out_port << endl;
      this_node.send_udp(out_port, response_str);
    }
    // Remove response object from outstanding_frame
    this_node.remove_outstanding_frame(resp_obj);
  }
}

/*
Forwards a frame to a node's neighbours if they haven't seen it before.

Determines whether a neighbour has seen a frame before by checking if they
are in its src string, if they are not then the frame is forwarded to them.
If all of a node's neighbours have seen the frame before, a response is immediately
provided to the sender, whether it be another node or the browser client.

Args:
  this_node: instance of the node class representing a given node in the network.
  out_frame: Frame object representing the frame to be forwarded.
Returns:
  void
*/
void send_frame_to_neighbours(Node &this_node, Frame &out_frame) {
  int sent_frames = 0;
  uint16_t out_port;
  string sender_name;
  if (out_frame.origin == this_node.name) {
    sender_name = this_node.name;
  } else {
    sender_name = out_frame.src.back();
  }
  out_frame.src.push_back(this_node.name);
  int start_time;
  if (out_frame.time == -1) {
    start_time = current_time();
    // start_time = 500;
  } else {
    start_time = out_frame.time;
  }
  std::unordered_set<string> src_set;
  // Add src list to set for (almost) constant time checks
  std::copy(out_frame.src.begin(), out_frame.src.end(),
            std::inserter(src_set, src_set.end()));
  for (auto neighbour : this_node.neighbours) {
    if (src_set.find(neighbour.second) == src_set.end()) {
      // We haven't sent the frame here before
      int arrival_time =
          this_node.find_arrival_time(neighbour.second, start_time);
      if (arrival_time == -1) {
        // Don't send frame if we can't get there today
        continue;
      }
      out_frame.time = arrival_time;
      string out_str = out_frame.to_string();
      this_node.send_udp(neighbour.first, out_str);
      cout << endl
           << this_node.name << " sent frame " << out_str << " to "
           << neighbour.second << endl;
      sent_frames = sent_frames + 1;
      ++this_node.packet_count;
    }
  }
  if (sent_frames == 0) {
    if (out_frame.origin == this_node.name) {
      // We can't get anywhere today, respond to browser
      cout << "Sending rejection to browser" << endl;
      int out_socket = this_node.response_sockets[out_frame.seqno];
      list<string> http_strings(
          {"Arrival time at destination: Couldn't get there",
           "Next leg of trip: None"});
      string http_response = http_string(200, "OK", http_strings);
      this_node.send_tcp(out_socket, http_response);
      this_node.remove_socket(out_socket);
    } else {
      // Sending a response to the node who sent it to us
      Frame response_frame =
          Frame(out_frame.dest, out_frame.origin, out_frame.src,
                out_frame.seqno, -1, RESPONSE);
      out_port = this_node.get_port_from_name(sender_name);
      string response_str = response_frame.to_string();
      this_node.send_udp(out_port, response_str);
    }
  } else {
    // We need to keep a track of the frames we sent out
    Response outstanding_response =
        Response(sent_frames, out_frame.src, out_frame.origin, out_frame.seqno);
    this_node.outstanding_frames.push_back(outstanding_response);
  }
}

/*
Processes incoming UDP packets.

Handles UDP packets by either forwarding them to a node's neighbours, responding
directly to the sender, or responding to the browser.

Args:
  this_node: instance of the node class representing a given node in the network.
  transmission: tuple containing the bytes of a UDP packet and origin port
Returns:
  None
*/
void process_udp(Node &this_node, string &transmission, uint16_t port) {
  Frame in_frame = Frame();
  in_frame.from_string(transmission);
  this_node.check_timetable();
  if (in_frame.type == NAME_FRAME) {
    this_node.neighbours[port] = in_frame.origin;
  } else if (in_frame.type == REQUEST) {
    process_request_frame(this_node, in_frame);
  } else {
    process_response_frame(this_node, in_frame);
  }
}