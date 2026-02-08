#include "drivers/blockdev.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "list.h"

static struct elevator_type *elevator_find(const char *name);

static void noop_add_req(struct request_queue *q, struct request *rq, int where) {
    struct elevator_queue *e = q->elevator;
    if (where == ELEVATOR_INSERT_FRONT)
        list_add(&rq->queuelist, &e->queue_head);
    else
        list_add_tail(&rq->queuelist, &e->queue_head);
}

static struct request *noop_next_req(struct request_queue *q) {
    struct elevator_queue *e = q->elevator;
    if (list_empty(&e->queue_head)) return 0;
    struct request *rq = list_entry(e->queue_head.next, struct request, queuelist);
    return rq;
}

static int noop_merge_req(struct request_queue *q, struct request *req, struct bio *bio) {
    (void)q;
     
    if (req->sector + req->nr_sectors == bio->sector) {
        if (req->bio) {
             struct bio *b = req->bio;
             while (b->next) b = b->next;
             b->next = bio;
        } else {
             req->bio = bio;
        }
        req->nr_sectors += (bio->size >> 9);
        return 1;
    }
    return 0;
}

static struct elevator_ops noop_ops = {
    .elevator_merge_fn = noop_merge_req,
    .elevator_add_req_fn = noop_add_req,
    .elevator_next_req_fn = noop_next_req,
};

static struct elevator_type noop_iosched = {
    .ops = &noop_ops,
    .elevator_name = "noop",
    .elevator_owner = 0
};

static void deadline_add_req(struct request_queue *q, struct request *rq, int where) {
    struct elevator_queue *e = q->elevator;
    
    if (where == ELEVATOR_INSERT_FRONT) {
        list_add(&rq->queuelist, &e->queue_head);
        return;
    }

    struct list_head *pos;
    
    list_for_each(pos, &e->queue_head) {
        struct request *curr = list_entry(pos, struct request, queuelist);
        if (rq->sector < curr->sector) {
            list_add(&rq->queuelist, pos->prev);
            return;
        }
    }
    list_add_tail(&rq->queuelist, &e->queue_head);
}

static struct request *deadline_next_req(struct request_queue *q) {
    struct elevator_queue *e = q->elevator;
    if (list_empty(&e->queue_head)) return 0;
    return list_entry(e->queue_head.next, struct request, queuelist);
}

static int deadline_merge_req(struct request_queue *q, struct request *req, struct bio *bio) {
    (void)q;
     
    if (req->sector + req->nr_sectors == bio->sector) {
        if (req->bio) {
             struct bio *b = req->bio;
             while (b->next) b = b->next;
             b->next = bio;
        } else {
             req->bio = bio;
        }
        req->nr_sectors += (bio->size >> 9);  
        return 1;  
    }
    return 0;  
}

static struct elevator_ops deadline_ops = {
    .elevator_merge_fn = deadline_merge_req,
    .elevator_add_req_fn = deadline_add_req,
    .elevator_next_req_fn = deadline_next_req,
};

static struct elevator_type deadline_iosched = {
    .ops = &deadline_ops,
    .elevator_name = "deadline",
    .elevator_owner = 0
};

static struct elevator_type *elevator_find(const char *name) {
    if (strcmp(name, "noop") == 0) return &noop_iosched;
    if (strcmp(name, "deadline") == 0) return &deadline_iosched;
    return &noop_iosched;  
}

int elevator_init(struct request_queue *q, char *name) {
    struct elevator_type *e_type = elevator_find(name);
    if (!e_type) return -1;
    
    struct elevator_queue *eq = (struct elevator_queue*)kmalloc(sizeof(struct elevator_queue));
    if (!eq) return -1;
    
    memset(eq, 0, sizeof(struct elevator_queue));
    eq->type = e_type;
    eq->ops = e_type->ops;
    INIT_LIST_HEAD(&eq->queue_head);
    
    q->elevator = eq;
    return 0;
}

void elevator_exit(struct elevator_queue *e) {
    if (e) kfree(e);
}

void __elv_add_request(struct request_queue *q, struct request *rq, int where) {
    if (q->elevator && q->elevator->ops->elevator_add_req_fn) {
        q->elevator->ops->elevator_add_req_fn(q, rq, where);
    }
}

struct request *elv_next_request(struct request_queue *q) {
    if (q->elevator && q->elevator->ops->elevator_next_req_fn) {
        return q->elevator->ops->elevator_next_req_fn(q);
    }
    return 0;
}

int elv_merge_bio(struct request_queue *q, struct bio *bio) {
    if (!q->elevator || !q->elevator->ops->elevator_merge_fn) return 0;

    struct list_head *pos;
    struct elevator_queue *e = q->elevator;
    
    if (!list_empty(&e->queue_head)) {
        list_for_each(pos, &e->queue_head) {
             struct request *curr = list_entry(pos, struct request, queuelist);
             if (q->elevator->ops->elevator_merge_fn(q, curr, bio)) {
                 return 1;
             }
        }
    }
    return 0;
}
