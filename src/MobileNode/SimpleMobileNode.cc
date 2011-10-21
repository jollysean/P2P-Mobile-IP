// Author: Thaddeus Diamond (diamond@cs.yale.edu)
//
// This is the implementation file for a simple mobile node.

#include <iostream>
#include "MobileNode/SimpleMobileNode.h"

void SimpleMobileNode::Run() {
  tunnel_fd_ = CreateTunnel(tunnel_name_);
  ConnectToHome(home_port_, NULL, true);

  Signal::HandleSignalInterrupts();
  do {
    CollectOutgoingTraffic();
    ChangeHomeIdentity();
  } while (Signal::ShouldContinue());

  ShutDown(true, "Normal termination.");
}

void SimpleMobileNode::ShutDown(bool should_exit, const char* format, ...) {
  fprintf(stdout, "Shutting Down Mobile Node... ");

  va_list arguments;
  va_start(arguments, format);
  fprintf(stderr, format, arguments);
  perror(" ");

  fprintf(stdout, "OK\n");
  if (should_exit)
    exit(1);
}

int SimpleMobileNode::CreateTunnel(char* tunnel) {
  int tunnel_fd = open("/dev/net/tun", O_RDWR, 0);
  if (tunnel_fd < 0)
    ShutDown(true, "There was an error opening the virtual tunnel.");
    
  struct ifreq virtual_interface;
  memset(&virtual_interface, 0, sizeof(virtual_interface));
  virtual_interface.ifr_flags = IFF_TUN;
  
  char interface_name[IFNAMSIZ] = "tun0";
  strncpy(virtual_interface.ifr_name, interface_name, IFNAMSIZ);

  if (ioctl(tunnel_fd, TUNSETIFF, (void*) &virtual_interface) < 0)
    ShutDown(true, "Error performing ioctl on virtual tunnel");
  
  strncpy(tunnel, virtual_interface.ifr_name,
          strlen(virtual_interface.ifr_name));

  // TODO(Thad): Set the tunnel interface to the right number
  //    i.e. tunnel_interface_ = tunnel_interface;

  return tunnel_fd;
}

void SimpleMobileNode::CollectOutgoingTraffic() {
/* TODO(Thad): This should collect from the virtual tunnel

  // First, we iterate through all the sockets that have been registerd
  map<int, int>::iterator it;
  for (it = app_tunnels_.begin(); it != app_tunnels_.end(); it++) {
    char buffer[4096];
    memset(&buffer, 0, sizeof(buffer));
    
    // Next, we read from the virtual tunnels into our buffer and send along
    if (read((*it).first, buffer, sizeof(buffer)) > 0) {
      ConnectToHome(data_port_, buffer);
      fprintf(stdout, "Virtual tunnel received from application #%d: %s\n",
              (*it).second, buffer);
    }
  }*/
}

void SimpleMobileNode::ConnectToHome(unsigned short port, char* data, 
                                     bool initial) {
  struct hostent* home_entity;
  if (!(home_entity = gethostbyname(home_ip_address_)))
    ShutDown(true, "Failed to get home agent at specified IP");

  struct sockaddr_in peer_in;
  memset(&peer_in, 0, sizeof(peer_in));
  peer_in.sin_family = domain_;
  peer_in.sin_addr.s_addr = ((struct in_addr *) (home_entity->h_addr))->s_addr;
  peer_in.sin_port = htons(port);

  int connection_socket = socket(domain_, transmission_type_, protocol_);
  if (connection_socket < 0)
    ShutDown(true, "Failed to open a socket");

  struct sockaddr_in socket_in;
  memset(&socket_in, 0, sizeof(socket_in));
  socket_in.sin_family = domain_;
  socket_in.sin_addr.s_addr = INADDR_ANY;
  socket_in.sin_port = htons(listener_port_);

  int on = 1;
  if (setsockopt(connection_socket, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<char*>(&on), sizeof(on)) < 0)
    ShutDown(true, "Could not make the socket reusable");
  if (bind(connection_socket, (struct sockaddr*) &socket_in, sizeof(socket_in)))
    ShutDown(true, "Error binding socket to send to home agent on port %d via %d",
        port, listener_port_);
  if (connect(connection_socket, (struct sockaddr*) &peer_in, sizeof(peer_in)))
    ShutDown(true, "Failed to connect to the home agent");
    
  if (initial) {
    char buffer[4096];
    memset(&buffer, 0, sizeof(buffer));
    if (read(connection_socket, buffer, sizeof(buffer)) < 1)
      ShutDown(true, "Failed to get a permanent address from home agent.");
    
    permanent_address_ = atoi(buffer);
    permanent_port_ = atoi(buffer + 20);
    
    fprintf(stdout, "Now registered at home agent w/permanent address %d:%d\n",
            permanent_address_, permanent_port_);
  }

  if (data) {
    if (write(connection_socket, data, strlen(data)) < 0)
      ShutDown(true, "Error sending message to home agent");
  } else {
    fprintf(stdout, "Connected to host entity on port %d, sending via %d\n",
            port, ntohs(socket_in.sin_port));
  }

  close(connection_socket);
}

void SimpleMobileNode::ChangeHomeIdentity() {
  int current_ip_address = GetCurrentIPAddress();

  if (current_ip_address != last_known_ip_address_) {
    char buffer[31];
    snprintf(buffer, sizeof(buffer), "%-20d%-10d", last_known_ip_address_,
             listener_port_);

    ConnectToHome(change_port_, buffer);
    last_known_ip_address_ = current_ip_address;

    fprintf(stdout, "Changed IP address.  Now at %d\n", current_ip_address);
  }
}

int SimpleMobileNode::GetCurrentIPAddress() const {
  struct ifaddrs *if_address, *if_struct;
  getifaddrs(&if_struct);

  for (if_address = if_struct; if_address != NULL; 
       if_address = if_address->ifa_next) {
    struct sockaddr_in* address = (struct sockaddr_in *) if_address->ifa_addr;

    if (address != NULL && ((address->sin_addr.s_addr << 24) >> 24) > 127) {
      int ip_number = address->sin_addr.s_addr;
      freeifaddrs(if_struct);
      return ip_number;
    }
  }

  return -1;
}