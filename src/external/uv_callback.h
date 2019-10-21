#ifndef UV_CALLBACK_H
#define UV_CALLBACK_H
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <uv.h>


/* Typedefs */

typedef struct uv_callback_s   uv_callback_t;
typedef struct uv_call_s       uv_call_t;


/* Callback Functions */

typedef void* (*uv_callback_func)(uv_callback_t* handle, void *data);


/* Functions */

int uv_callback_init(uv_loop_t* loop, uv_callback_t* callback, uv_callback_func function, int callback_type);

int uv_callback_init_ex(
   uv_loop_t* loop,
   uv_callback_t* callback,
   uv_callback_func function,
   int callback_type,
   void (*free_cb)(void*),
   void (*free_result)(void*)
);

int uv_callback_fire(uv_callback_t* callback, void *data, uv_callback_t* notify);

int uv_callback_fire_ex(uv_callback_t* callback, void *data, void (*free_data)(void*), uv_callback_t* notify);

int uv_callback_fire_sync(uv_callback_t* callback, void *data, void** presult, int timeout);

void uv_callback_stop(uv_callback_t* callback);
void uv_callback_stop_all(uv_loop_t* loop);

int uv_is_callback(uv_handle_t *handle);
void uv_callback_release(uv_callback_t *callback);


/* Constants */

#define UV_DEFAULT      0
#define UV_COALESCE     1


/* Structures */

struct uv_callback_s {
   uv_async_t async;          /* base async handle used for thread signal */
   void *data;                /* additional data pointer. not the same from the handle */
   int usequeue;              /* if this callback uses a queue of calls */
   uv_call_t *queue;          /* queue of calls to this callback */
   uv_mutex_t mutex;          /* mutex used to access the queue */
   uv_callback_func function; /* the function to be called */
   void *arg;                 /* data argument for coalescing calls (when not using queue) */
   uv_idle_t idle;            /* idle handle used to drain the queue if new async request was sent while an old one was being processed */
   int idle_active;           /* flags if the idle handle is active */
   uv_callback_t *master;     /* master callback handle, the one with the valid uv_async handle */
   uv_callback_t *next;       /* the next callback from this uv_async handle */
   int inactive;              /* this callback is no more valid. the called thread should not fire the response callback */
   int refcount;              /* reference counter */
   void (*free_cb)(void*);    /* function to release this object */
   void (*free_result)(void*);/* function to release the result of the call if not used */
};

struct uv_call_s {
   uv_call_t *next;           /* pointer to the next call in the queue */
   uv_callback_t *callback;   /* callback linked to this call */
   void *data;                /* data argument for this call */
   void (*free_data)(void*);  /* function to release the data if the call is not fired */
   uv_callback_t *notify;     /* callback to be fired with the result of this one */
};


#ifdef __cplusplus
}
#endif
#endif  // UV_CALLBACK_H

