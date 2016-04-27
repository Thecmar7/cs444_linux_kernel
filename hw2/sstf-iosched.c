#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/init.h>

/******************************************************************************
 * struct sstf_data
 *		- head_pos			:
 *		- direction			:	way the elevator
 *		- list_head queue	:	linked list
 *
 *
 ******************************************************************************/
struct sstf_data {
	sector_t head_pos;
	unsigned char direction;
	struct list_head queue;
};

/******************************************************************************
 * static void noop_merged_requests(struct request_queue *q,
 *									struct request *rq,
 *									struct request *next)
 *	This is the merged part
 *
 *
 ******************************************************************************/
static void noop_merged_requests(struct request_queue *q,
								 struct request *rq,
								 struct request *next)
{
	list_del_init(&next->queuelist);
}

/******************************************************************************
 * static int sstf_dispatch(struct request_queue *q, int force)
 *
 *
 ******************************************************************************/
static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *sd = q->elevator->elevator_data;
	
	if (!list_empty(&sd->queue)) {
		struct request *nextrq, *prevrq, *rq;
		
		nextrq = list_entry(sd->queue.next, struct request, queuelist);
		prevrq = list_entry(nextrq->queuelist.prev, struct request, queuelist);
		
		/* 0: going forward, 1: going back */
		if (nextrq == prevrq) {
			rq = nextrq;
		} else {
			if (sd->direction == 1) {
				if (nextrq->__sector > sd->head_pos) {
					rq = nextrq;
				} else {
					sd->direction = 0;
					rq = prevrq;
				}
			} else { 
				if (prevrq->__sector < sd->head_pos) {
					rq = prevrq;
				} else {
					sd->direction = 1;
					rq = nextrq;
				}
			}
		}
		
		list_del_init(&rq->queuelist);
		
		sd->head_pos = blk_rq_pos(rq) + blk_rq_sectors(rq);
		elv_dispatch_add_tail(q, rq);
		
		return 1;
	}
	return 0;
}


/******************************************************************************
 * static void sstf_add_request(struct request_queue *q, struct request *rq)
 *
 *
 ******************************************************************************/
static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;
	
	struct request *rnext, *rprev;
	sector_t next, rq_sector;
	
	if(list_empty(&sd->queue)) {
		list_add(&rq->queuelist,&sd->queue);
	} else {
		rnext = list_entry(sd->queue.next, struct request, queuelist);
		rprev = list_entry(sd->queue.prev, struct request, queuelist);
		
		next = blk_rq_pos(rnext);
		rq_sector = blk_rq_pos(rq);
		
		while(rq_sector > next){
			rnext = list_entry(sd->queue.next, struct request, queuelist);
			rprev = list_entry(sd->queue.prev, struct request, queuelist);
			next = blk_rq_pos(rnext);
		}
		
		__list_add(&rq->queuelist, &rprev->queuelist, &rnext->queuelist);
	}
}

/******************************************************************************
 * static struct request *noop_former_request(struct request_queue *q, 
 *											  struct request *rq)
 *
 *
 ******************************************************************************/
static struct request *noop_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;
	
	if (rq->queuelist.prev == &sd->queue) {
		return NULL;
	}
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

/******************************************************************************
 * static struct request *noop_latter_request(struct request_queue *q, 
 *											  struct request *rq)
 *
 *
 ******************************************************************************/
static struct request *noop_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;
	
	if (rq->queuelist.next == &sd->queue) {
		return NULL;
	}
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

/******************************************************************************
 * static void *noop_init_queue(struct request_queue *q)
 *
 *
 ******************************************************************************/
static void *noop_init_queue(struct request_queue *q)
{
	struct sstf_data *sd;
	
	sd = kmalloc_node(sizeof(*sd), GFP_KERNEL, q->node);
	if (!sd) {
		return NULL;
	}
	INIT_LIST_HEAD(&sd->queue);
	sd->head_pos = 0;
	sd->direction = HEAD_FWD;
	return sd;
}

/******************************************************************************
 * static void noop_exit_queue(struct elevator_queue *e)
 *
 *
 ******************************************************************************/
static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *sd = e->elevator_data;
	kfree(sd);
}

/******************************************************************************
 * static struct elevator_type elevator_sstf
 *
 *
 ******************************************************************************/
static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= noop_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= noop_former_request,
		.elevator_latter_req_fn		= noop_latter_request,
		.elevator_init_fn			= noop_init_queue,
		.elevator_exit_fn			= noop_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

/******************************************************************************
 * static int __init sstf_init(void)
 *
 *
 ******************************************************************************/
static int __init sstf_init(void)
{
	elv_register(&elevator_sstf);
	return 0;
}

/******************************************************************************
 * static void __exit sstf_exit(void)
 *
 *
 ******************************************************************************/
static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);

MODULE_AUTHOR("Cramer Smith");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF I/O scheduler");

