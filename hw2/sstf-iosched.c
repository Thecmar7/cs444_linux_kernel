/*
 * Author:		Cramer Smith
 * Class:		CS444 - Spring 2016
 * Assignment:	Assignment 2 - SSTF I/O Scheduler
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>

struct look_data {
    struct list_head queue;
    sector_t hp;
    int dir;
};

static void look_merged_requests
(struct request_queue *q, struct request *rq, struct request *next)
{
    list_del_init(&next->queuelist);
}

/* 
 look_dispatch
 */
static int look_dispatch(struct request_queue *q, int force)
{
    struct look_data *nd = q->elevator->elevator_data;
    printk("Here is the start of dispatch\n");

    if(!list_empty(&nd->queue)) {
        struct request *nxt_rq, *prv_rq, *rq;
	
    	nxt_rq = list_entry(nd->queue.next, struct request, queuelist);
    	prv_rq = list_entry(nd->queue.prev, struct request, queuelist);

    	if(prv_rq == nxt_rq) {
			printk("There is one request\n");
			rq = nxt_rq;
		} else {
			printk("There are mutliple requests\n");

    	    if(nd->dir == 1) {
				printk("Queue direction is going forward\n");
    		
				if(nxt_rq->__sector > nd->hp) {
					rq = nxt_rq;

				} else {
					nd->dir = 0;
					rq = prv_rq;
				}
			} else {
				printk("Queue direction is going backward\n");

				if(prv_rq->__sector < nd->hp) {
					rq = prv_rq;
				} else {
					nd->dir = 1;
					rq = nxt_rq;
				}
			}
		}

		printk("<NOW EXECUTING>\n");
        //Remove it from the queue we just got it from
        list_del_init(&rq->queuelist);
        //get the new position for the read head
        nd->hp = blk_rq_pos(rq) + blk_rq_sectors(rq);
        //give request to the elevator 
        elv_dispatch_add_tail(q, rq);
        printk("Things have finished.\n");
        printk("LOOK accessing %llu\n",(unsigned long long) rq->__sector);
        return 1;
    }   
    return 0;
}


static void look_add_request(struct request_queue *q, struct request *rq)
{
    struct look_data *nd = q->elevator->elevator_data;
    struct request *nxt_rq, *prv_rq;
 
    printk("Starting add\n");

    if(list_empty(&nd->queue)) {
        printk("The list is empty\n");
        list_add(&rq->queuelist, &nd->queue);
    } else {
        printk("Locating position for req\n");

		nxt_rq = list_entry(nd->queue.next, struct request, queuelist);
		prv_rq = list_entry(nd->queue.prev, struct request, queuelist);

		while(blk_rq_pos(rq) > blk_rq_pos(nxt_rq)) {
			nxt_rq = list_entry(nxt_rq->queuelist.next, struct request, queuelist);
			prv_rq = list_entry(prv_rq->queuelist.prev, struct request, queuelist);
		}

		list_add(&rq->queuelist, &prv_rq->queuelist);
		printk("Found the position\n");
    }
    printk("SSTF adding %llu\n", (unsigned long long) rq->__sector);
}


static struct request * 
look_former_request(struct request_queue *q, struct request *rq)
{
    struct look_data *nd = q->elevator->elevator_data;

	if(rq->queuelist.prev == &nd->queue) {
        return NULL;
	}
    return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request * 
look_latter_request(struct request_queue *q, struct request *rq)
{
    struct look_data *nd = q->elevator->elevator_data;

	if(rq->queuelist.next == &nd->queue) {
        return NULL;
	}
    return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int look_init_queue(struct request_queue *q, struct elevator_type *e)
{
    struct look_data *nd;
    struct elevator_queue *eq;

    eq = elevator_alloc(q, e);
	if(!eq) {
        return -ENOMEM;
	}
		
    nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
    if(!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
    }
  
    nd->hp = 0;
    eq->elevator_data = nd;

    INIT_LIST_HEAD(&nd->queue);
    spin_lock_irq(q->queue_lock);
    q->elevator = eq;
    spin_unlock_irq(q->queue_lock);
    return 0;
}

static void look_exit_queue(struct elevator_queue *e)
{
    struct look_data *nd = e->elevator_data;

    BUG_ON(!list_empty(&nd->queue));
    kfree(nd);
}

static struct elevator_type elevator_look = {
    .ops = {
        .elevator_merge_req_fn = look_merged_requests,
        .elevator_dispatch_fn = look_dispatch,
        .elevator_add_req_fn = look_add_request,
        .elevator_former_req_fn = look_former_request,
        .elevator_latter_req_fn = look_latter_request,
        .elevator_init_fn = look_init_queue,
        .elevator_exit_fn = look_exit_queue,
    },
    .elevator_name = "sstf",
    .elevator_owner = THIS_MODULE,

};

static int __init look_init(void)
{
	return elv_register(&elevator_look);
}

static void __exit look_exit(void)
{
	elv_unregister(&elevator_look);
}

module_init(look_init);
module_exit(look_exit);


MODULE_AUTHOR("Cramer Smith");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF I/O scheduler -- CS 444 Spring 2016");
