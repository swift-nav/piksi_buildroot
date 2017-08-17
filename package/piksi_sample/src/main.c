// change to piksi_heartbeat_sample

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <getopt.h>
#include "piksi_sample.h"

#define MAXBUF 65536

static void callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
	printf("piksi's heart is beating...\n");
	char msg[10] = "HEARTBEAT";

	struct udp_broadcast_context* ctx = (struct udp_broadcast_context *) context;
	sendto(ctx->sock,msg,strlen(msg),0,(const struct sockaddr *)ctx->sock_in,sizeof(*(ctx->sock_in)));
	return;
}

void udp_bridge_setup(sbp_zmq_rx_ctx_t *rx_ctx, struct udp_broadcast_context *context)
{
	// arguments are as follows:
	// (1) rx_ctx from which to register the callback
	// (2) identifier for msg type to respond to (check libsbp/c/include/*.h for msg id's)
	// (3) callback function, must have specific construction as shown above
	// (4) context in which the callback function occurs (in this case, udp broadcast)
	// (5) node for registration - typically leave as NULL
	sbp_zmq_rx_callback_register(rx_ctx,SBP_MSG_HEARTBEAT,callback,(void *)context,NULL);
	printf("CALLBACK REGISTERED\n");
	return;
}

int main(int argc, char *argv[])
{
	logging_init(PROGRAM_NAME);
	zsys_handler_set(NULL);

	// set up the socket
	int sock;
	struct sockaddr_in sock_in;
	memset(&sock_in,0,sizeof(struct sockaddr_in));

	// initalize the socket
	sock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
	sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_in.sin_port = htons(0);
	sock_in.sin_family = PF_INET;

	// bind socket, set permissions
	int status = bind(sock,(struct sockaddr *) &sock_in,sizeof(struct sockaddr_in));
	piksi_log(LOG_INFO,"Bind Status = %d\n", status);
	int broadcastPermission = 1;
	status = setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&broadcastPermission,sizeof(int));
	piksi_log(LOG_INFO,"Setsockopt status = %d\n",status);

	// set socket to broadcast, at given port number
	sock_in.sin_addr.s_addr = htonl(-1); // 255.255.255.255
	sock_in.sin_port = htonl(atoi(argv[1]));
	sock_in.sin_family = PF_INET;

	// setup a callback context
	struct udp_broadcast_context cb_context;
	cb_context.sock = sock;
	cb_context.sock_in = &sock_in;

	// create a pubsub context, make bridge, setup the loop
	sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT,SUB_ENDPOINT);	
	udp_bridge_setup(sbp_zmq_pubsub_rx_ctx_get(ctx),&cb_context);
	zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

	// clean up and go home
	shutdown(sock,2);
	close(sock);
	sbp_zmq_pubsub_destroy(&ctx);
	exit(EXIT_SUCCESS);

}
