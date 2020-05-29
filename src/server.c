#include "client_registry.h"
#include "protocol.h"
#include "server.h"
#include "debug.h"
#include "trader.h"
#include "exchange.h"

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

extern EXCHANGE *exchange;
extern CLIENT_REGISTRY *client_registry;

void *brs_client_service(void *arg){
    //free arg
    int fd = *(int*)arg;
    free(arg);
    //detach thread
    pthread_detach(pthread_self());
    //register client fd
    creg_register(client_registry, fd);
    //malloc a dummy packet, payload, status, trader
    BRS_PACKET_HEADER* temp_pkt = malloc(sizeof(BRS_PACKET_HEADER));
    void** payloadp = malloc(sizeof(payloadp));
    TRADER *my_trader = NULL;
    BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
    //EXCHANGE* exchange = malloc(sizeof(EXCHANGE));
    //struct for sec and nsec
    struct timespec time;
    //service loop stops when network connection is shut down and EOF is recieved 
    while(1){
        int my_pkt = proto_recv_packet(fd, temp_pkt, payloadp);        
        if(my_pkt != -1){
            if(temp_pkt -> type == BRS_LOGIN_PKT){
                if(my_trader == NULL){
                    debug("login packet received from fd %d", fd);
                    my_trader = trader_login(fd, *payloadp);
                    if(my_trader == NULL){
                        temp_pkt -> type = BRS_NACK_PKT;
                        temp_pkt -> size = 0;            
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        trader_send_packet(my_trader, temp_pkt, NULL);
                    }    
                    //prepare packet to send back
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    temp_pkt -> type = BRS_ACK_PKT;
                    temp_pkt -> size = 0;       //no payload
                    temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    proto_send_packet(fd, temp_pkt, NULL);
                } else {
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    temp_pkt -> type = BRS_NACK_PKT;
                    temp_pkt -> size = 0;            
                    temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    trader_send_packet(my_trader, temp_pkt, NULL);
                }
            } else if(temp_pkt -> type == BRS_STATUS_PKT){
                if(my_trader != NULL){
                    //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                    debug("status packet received from fd %d", fd);
                    temp_pkt -> type = BRS_ACK_PKT;
                    temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                    temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    //status -> orderid = 0; 
                    exchange_get_status(exchange, status);       
                    trader_send_ack(my_trader, status);
                    //free(status);
                }
            } else if(temp_pkt -> type == BRS_DEPOSIT_PKT){
                if(my_trader != NULL){
                    //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                    debug("Deposit packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    temp_pkt -> type = BRS_ACK_PKT;
                    temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                    temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    //get payload of packet (amount, brs_funds_info) and pass it to trader_increase balance 
                    BRS_FUNDS_INFO *funds = *payloadp;
                    trader_increase_balance(my_trader, ntohl(funds -> amount)); //hard coded value
                    funds_t my_amount = funds -> amount;
                    status -> balance = (status -> balance) + my_amount;
                    //status -> orderid = 0;
                    exchange_get_status(exchange, status);
                    trader_send_packet(my_trader, temp_pkt, status);
                    //free(status);
                }
            } else if(temp_pkt -> type == BRS_WITHDRAW_PKT){
                if(my_trader != NULL){
                    debug("withdraw packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    //get payload of packet (amount, brs_funds_info) and pass it to trader_increase balance 
                    BRS_FUNDS_INFO *funds = *payloadp;
                    if(trader_decrease_balance(my_trader, ntohl(funds -> amount)) == -1){   //hard coded value
                        //insufficent funds, make no change, respond with a nack packet
                        debug("insufficent funds");
                        temp_pkt -> type = BRS_NACK_PKT;
                        temp_pkt -> size = 0;            
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        trader_send_packet(my_trader, temp_pkt, NULL);
                    } else {
                        //success
                        //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                        temp_pkt -> type = BRS_ACK_PKT;
                        temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        funds_t my_amount = funds -> amount;
                        status -> balance = status -> balance - my_amount;
                        // status -> orderid = 0;
                        exchange_get_status(exchange, status);
                        trader_send_packet(my_trader, temp_pkt, status);
                        //free(status);
                    }
                }
            } else if(temp_pkt -> type == BRS_ESCROW_PKT){
                if(my_trader != NULL){
                    //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                    debug("excrow packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    temp_pkt -> type = BRS_ACK_PKT;
                    temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                    temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    //get payload of packet (quantity, brs_escrow_info) and pass it to increase_inventory
                    BRS_ESCROW_INFO *escrow = *payloadp;
                    trader_increase_inventory(my_trader, ntohl(escrow -> quantity));
                    quantity_t my_quantity = escrow -> quantity;
                    status -> inventory = status -> inventory + my_quantity;
                    // status -> orderid = 0;
                    exchange_get_status(exchange, status);
                    trader_send_packet(my_trader, temp_pkt, status);
                    //free(status);
                }
            } else if(temp_pkt -> type == BRS_RELEASE_PKT){
                if(my_trader != NULL){
                    debug("RELEASE packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    //get payload of packet (quantity, brs_escrow_info) and pass it to decrease_inventory
                    BRS_ESCROW_INFO *escrow = *payloadp;
                    if(trader_decrease_inventory(my_trader, ntohl(escrow -> quantity)) == -1){
                        //insufficent escrow, make no change, respond with a nack packet
                        temp_pkt -> type = BRS_NACK_PKT;
                        temp_pkt -> size = 0;            
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        trader_send_packet(my_trader, temp_pkt, NULL);
                    } else {
                        //success
                        //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                        temp_pkt -> type = BRS_ACK_PKT;
                        temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        quantity_t my_quantity = escrow -> quantity;
                        status -> inventory = status -> inventory - my_quantity;
                        //status -> orderid = 0;
                        exchange_get_status(exchange, status);
                        trader_send_packet(my_trader, temp_pkt, status);
                        //free(status);
                    }
                }
            } else if(temp_pkt -> type == BRS_BUY_PKT){
                if(my_trader != NULL){    
                    debug("BUY packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    //get payload of packet (quantity and price, brs_order_info) 
                    BRS_ORDER_INFO *order = *payloadp;
                    funds_t my_price = order -> price;
                    quantity_t my_quantity = order -> quantity;
                    //long int my_total_price = my_price * my_quantity;
                    // if(trader_decrease_balance(my_trader, ntohl(my_total_price)) == -1){    //amount
                    //     //insufficent funds, send nack packet
                    //     temp_pkt -> type = BRS_NACK_PKT;
                    //     temp_pkt -> size = 0;            
                    //     temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    //     temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    //     trader_send_packet(my_trader, temp_pkt, NULL);
                    // } else {
                        //success
                        orderid_t order_id;
                        if((order_id = exchange_post_buy(exchange, my_trader, ntohl(order -> quantity), ntohl(order -> price))) == 0){ //quantity, price
                            //return 0 send nack
                            temp_pkt -> type = BRS_NACK_PKT;
                            temp_pkt -> size = 0;            
                            temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                            temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                            trader_send_packet(my_trader, temp_pkt, NULL);
                        } else {
                            //success
                            //is this function correct?
                            //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                            temp_pkt -> type = BRS_ACK_PKT;
                            temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                            temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                            temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                            status -> orderid = htonl(order_id);       
                            //debug("order_id = %u", order_id);
                            for(int i = 0; i < ntohl(my_quantity); i++){
                                status -> balance = status -> balance - my_price;
                            }
                            exchange_get_status(exchange, status);
                            trader_send_packet(my_trader, temp_pkt, status);
                            //free(status);
                        }
                    }
                //}
            } else if(temp_pkt -> type == BRS_SELL_PKT){
                if(my_trader != NULL){    
                    debug("SELL packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    //get payload of packet (quantity and price, brs_order_info)
                    BRS_ORDER_INFO *order = *payloadp;
                    // if(trader_decrease_inventory(my_trader, ntohl(order -> quantity)) == -1){  //quantity
                    //     //insufficent inventory
                    //     temp_pkt -> type = BRS_NACK_PKT;
                    //     temp_pkt -> size = 0;            
                    //     temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                    //     temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                    //     trader_send_packet(my_trader, temp_pkt, NULL);
                    // } else {
                        orderid_t order_id;
                        if((order_id = exchange_post_sell(exchange, my_trader, ntohl(order -> quantity), ntohl(order -> price))) == 0){ //quantity, price
                            //failure send a nack packet
                            temp_pkt -> type = BRS_NACK_PKT;
                            temp_pkt -> size = 0;            
                            temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                            temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                            trader_send_packet(my_trader, temp_pkt, NULL);
                        } else {
                            //success
                            //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                            temp_pkt -> type = BRS_ACK_PKT;
                            temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                            temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                            temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                            status -> orderid = htonl(order_id);
                            quantity_t my_quantity = order -> quantity;
                            // long int my_price = order -> price;
                            status -> inventory = status -> inventory - my_quantity;    
                            //increase balance, buy how much?
                            // status -> balance =  
                            exchange_get_status(exchange, status);
                            trader_send_packet(my_trader, temp_pkt, status);
                            //free(status);
                        }
                    }
                //}
            } else if(temp_pkt -> type == BRS_CANCEL_PKT){
                if(my_trader != NULL){
                    debug("CANCEL packet received from fd %d", fd);
                    clock_gettime(CLOCK_MONOTONIC, &time);
                    //get payload (order, brs_cancel_info)
                    BRS_CANCEL_INFO *cancel = *payloadp;
                    //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                    if(exchange_cancel(exchange, my_trader, ntohl(cancel -> order), &status -> quantity) == -1){   //order, quantity ntohl(status -> quantity)
                        //nothing to cancel, send nack
                        temp_pkt -> type = BRS_NACK_PKT;
                        temp_pkt -> size = 0;            
                        temp_pkt -> timestamp_sec = time.tv_sec;      
                        temp_pkt -> timestamp_nsec = time.tv_nsec;
                        trader_send_packet(my_trader, temp_pkt, NULL);
                        //free(status);
                    } else {
                        //finish this
                        //BRS_STATUS_INFO* status = malloc(sizeof(BRS_STATUS_INFO));
                        temp_pkt -> type = BRS_ACK_PKT;
                        temp_pkt -> size = htons(sizeof(BRS_STATUS_INFO));      
                        temp_pkt -> timestamp_sec = htonl(time.tv_sec);      
                        temp_pkt -> timestamp_nsec = htonl(time.tv_nsec);
                        status -> orderid = cancel -> order;
                        exchange_get_status(exchange, status);
                        trader_send_packet(my_trader, temp_pkt, status);
                        //free(status);
                    }
                }
            }
        } else {
            //terminate function?
            debug("*********GETS IN HERE ***********");
            //debug("balance %d", status -> balance);
            trader_logout(my_trader);
            free(temp_pkt);
            free(status);
            free(payloadp);
            creg_unregister(client_registry, fd);
            close(fd);
            pthread_cancel(pthread_self());
            break;
        }
    }
    return NULL;
}
