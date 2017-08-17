// list of includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <libsbp/system.h>
// #include <libsbp/navigation.h>

#define PROGRAM_NAME "piksi_sample"
// find port values at piksi_buildroot/zmq_router/src/zmq_router_sbp.c
#define PUB_ENDPOINT ">tcp://127.0.0.1:43061"
#define SUB_ENDPOINT ">tcp://127.0.0.1:43060"

// type definitions

struct udp_broadcast_context {
	int sock;
	struct sockaddr_in* sock_in;
};

// function prototypes

static void callback(u16 sender_id,u8 len, u8 msg_[], void *context);
void udp_bridge_setup(sbp_zmq_rx_ctx_t *rx_ctx, struct udp_broadcast_context* context);