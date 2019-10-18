#include <stdlib.h>
#include "uv_callback.h"

// not covered now: closing a uv_callback handle does not release all the resources
// automatically.
// for this libuv should support calling a callback when our handle is being closed.
// for now we must use the uv_callback_stop or .._stop_all before closing the event
// loop and then call uv_callback_release on the callback from uv_close.

/*****************************************************************************/
/* RECEIVER / CALLED THREAD **************************************************/
/*****************************************************************************/

#ifndef container_of
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

void uv_callback_idle_cb(uv_idle_t* handle);

/* Master Callback ***********************************************************/

int uv_is_callback(uv_handle_t *handle) {
   return (handle->type == UV_ASYNC && handle->data == handle);
}

void master_on_walk(uv_handle_t *handle, void *arg) {
   if (handle->type == UV_ASYNC && ((uv_callback_t*)handle)->usequeue) {
      *(uv_callback_t**)arg = (uv_callback_t *) handle;
   }
}

uv_callback_t * get_master_callback(uv_loop_t *loop) {
   uv_callback_t *callback=0;
   uv_walk(loop, master_on_walk, &callback);
   return callback;
}

/* Callback Release **********************************************************/

void uv_callback_release(uv_callback_t *callback) {
   if (callback) {
      callback->refcount--;
      if (callback->refcount == 0 && callback->free_cb) {
         /* remove the object from the list */
         uv_callback_t *cb = callback->master;
         while (cb) {
            if (cb->next == callback) {
               cb->next = callback->next;
               break;
            }
            cb = cb->next;
         }
         /* stop the idle handle */
         if (callback->idle_active) {
            uv_idle_stop(&callback->idle);
         }
         /* release the object */
         callback->free_cb(callback);
      }
   }
}

/* Dequeue *******************************************************************/

void * dequeue_call(uv_callback_t* callback) {
   uv_call_t *current, *prev = NULL;

   uv_mutex_lock(&callback->mutex);

   current = callback->queue;
   while (current && current->next) {
      prev = current;
      current = current->next;
   }

   if (prev)
      prev->next = NULL;
   else
      callback->queue = NULL;

   uv_mutex_unlock(&callback->mutex);

   return current;
}

void dequeue_all_from_callback(uv_callback_t* master, uv_callback_t* callback) {
   uv_call_t *call, *prev = NULL;

   if (!master) master = callback;

   uv_mutex_lock(&master->mutex);

   call = master->queue;
   while (call) {
      if (call->callback == callback) {
         /* remove it from the queue */
         if (prev)
            prev->next = call->next;
         else
            callback->queue = NULL;
         /* discard this call */
         if (call->data && call->free_data) {
            call->free_data(call->data);
         }
         free(call);
         call = prev->next;
      } else {
         prev = call;
         call = call->next;
      }
   }

   uv_mutex_unlock(&master->mutex);

}

/* Callback Function Call ****************************************************/

void uv_callback_async_cb(uv_async_t* handle) {
   uv_callback_t* callback = (uv_callback_t*) handle;

   if (callback->usequeue) {
      uv_call_t *call = dequeue_call(callback);
      if (call) {
         void *result = call->callback->function(call->callback, call->data);
         /* check if the result notification callback is still active */
         if (call->notify && !call->notify->inactive) {
            uv_callback_fire(call->notify, result, NULL);
         } else if (result && call->callback->free_result) {
            call->callback->free_result(result);
         }
         if (call->notify) {
            uv_callback_release(call->notify);
         }
         free(call);
         /* don't check for new calls now to prevent the loop from blocking
         for i/o events. start an idle handle to call this function again */
         if (!callback->idle_active) {
            uv_idle_start(&callback->idle, uv_callback_idle_cb);
            callback->idle_active = 1;
         }
      } else {
         /* no more calls in the queue. stop the idle handle */
         uv_idle_stop(&callback->idle);
         callback->idle_active = 0;
      }
   } else {
      callback->function(callback, callback->arg);
   }

}

void uv_callback_idle_cb(uv_idle_t* handle) {
   uv_callback_t* callback = container_of(handle, uv_callback_t, idle);
   uv_callback_async_cb((uv_async_t*)callback);
}

/* Initialization ************************************************************/

int uv_callback_init_ex(
   uv_loop_t* loop,
   uv_callback_t* callback,
   uv_callback_func function,
   int callback_type,
   void (*free_cb)(void*),
   void (*free_result)(void*)
){
   int rc;

   if (!loop || !callback || !function) return UV_EINVAL;

   memset(callback, 0, sizeof(uv_callback_t));
   callback->async.data = callback; /* mark as a uv_callback handle */

   callback->function = function;

   callback->refcount = 1;
   callback->free_cb = free_cb;

   switch(callback_type) {
   case UV_DEFAULT:
      callback->usequeue = 1;
      callback->free_result = free_result;
      callback->master = get_master_callback(loop);
      if (callback->master) {
         /* add this callback to the list */
         uv_callback_t *base = callback->master;
         while (base->next) { base = base->next; }
         base->next = callback;
         return 0;  /* the uv_async handle is already initialized */
      } else {
         uv_mutex_init(&callback->mutex);
         rc = uv_idle_init(loop, &callback->idle);
         if (rc) return rc;
      }
      /* fallthrough */
   case UV_COALESCE:
      break;
   default:
      return UV_EINVAL;
   }

   return uv_async_init(loop, (uv_async_t*) callback, uv_callback_async_cb);
}

int uv_callback_init(uv_loop_t* loop, uv_callback_t* callback, uv_callback_func function, int callback_type) {
   return uv_callback_init_ex(loop, callback, function, callback_type, NULL, NULL);
}

void uv_callback_stop(uv_callback_t* callback) {

   if (!callback) return;

   callback->inactive = 1;

   if (callback->usequeue) {
      dequeue_all_from_callback(callback->master, callback);
   }

}

void stop_all_on_walk(uv_handle_t *handle, void *arg) {
   if (uv_is_callback(handle)) {
      uv_callback_t *callback = (uv_callback_t *) handle;
      while (callback) {
         uv_callback_t *next = callback->next;
         uv_callback_stop(callback);
         callback = next;
      }
   }
}

void uv_callback_stop_all(uv_loop_t* loop) {
   uv_walk(loop, stop_all_on_walk, NULL);
}

/*****************************************************************************/
/* SENDER / CALLER THREAD ****************************************************/
/*****************************************************************************/

/* Asynchronous Callback Firing **********************************************/

int uv_callback_fire_ex(uv_callback_t* callback, void *data, void (*free_data)(void*), uv_callback_t* notify) {

   if (!callback) return UV_EINVAL;
   if (callback->inactive) return UV_EPERM;

   /* if there is a notification callback set, then the call must use a queue */
   if (notify && !callback->usequeue) return UV_EINVAL;

   if (callback->usequeue) {
      /* allocate a new call info */
      uv_call_t *call = malloc(sizeof(uv_call_t));
      if (!call) return UV_ENOMEM;
      /* save the call info */
      call->data = data;
      call->notify = notify;
      call->callback = callback;
      call->free_data = free_data;
      /* if there is a master callback, use it */
      if (callback->master) callback = callback->master;
      /* add the call to the queue */
      uv_mutex_lock(&callback->mutex);
      call->next = callback->queue;
      callback->queue = call;
      uv_mutex_unlock(&callback->mutex);
      /* increase the reference counter */
      if (notify) notify->refcount++;
   } else {
      callback->arg = data;
   }

   /* call uv_async_send */
   return uv_async_send((uv_async_t*)callback);
}

int uv_callback_fire(uv_callback_t* callback, void *data, uv_callback_t* notify) {
   return uv_callback_fire_ex(callback, data, NULL, notify);
}

#if 0
/* Synchronous Callback Firing ***********************************************/

struct call_result {
   int timed_out;
   int called;
   void *data;
};

void callback_on_close(uv_handle_t *handle) {
   if (uv_is_callback(handle)) {
      uv_callback_release((uv_callback_t*) handle);
   }
}

void callback_on_walk(uv_handle_t *handle, void *arg) {
   uv_close(handle, callback_on_close);
}

void * on_call_result(uv_callback_t *callback, void *data) {
   uv_loop_t *loop = ((uv_handle_t*)callback)->loop;
   struct call_result *result = loop->data;
   result->called = 1;
   result->data = data;
   uv_stop(loop);
   return NULL;
}

void on_timer(uv_timer_t *timer) {
   uv_loop_t *loop = timer->loop;
   struct call_result *result = loop->data;
   result->timed_out = 1;
   uv_stop(loop);
}

int uv_callback_fire_sync(uv_callback_t* callback, void *data, void** presult, int timeout) {
   struct call_result result = {0};
   uv_loop_t loop;
   uv_timer_t timer;
   uv_callback_t *notify;  /* must be allocated because it is shared with the called thread */
   int rc=0;

   if (!callback || callback->usequeue==0) return UV_EINVAL;

   notify = malloc(sizeof(uv_callback_t));
   if (!notify) return UV_ENOMEM;

   /* set the call result */
   uv_loop_init(&loop);
   uv_callback_init_ex(&loop, notify, on_call_result, UV_DEFAULT, free, NULL);
   loop.data = &result;

   /* fire the callback on the other thread */
   rc = uv_callback_fire(callback, data, notify);
   if (rc) {
      uv_close((uv_handle_t*) notify, callback_on_close);
      goto loc_exit;
   }

   /* if a timeout is supplied, set a timer */
   if (timeout > 0) {
      uv_timer_init(&loop, &timer);
      uv_timer_start(&timer, on_timer, timeout, 0);
   }

   /* run the event loop */
   uv_run(&loop, UV_RUN_DEFAULT);

   /* exited the event loop */
   /* before closing the loop handles */
   //uv_callback_stop(notify);
   uv_callback_stop_all(&loop);
   uv_walk(&loop, callback_on_walk, NULL);
   uv_run(&loop, UV_RUN_DEFAULT);
loc_exit:
   uv_loop_close(&loop);

   /* store the result */
   if (presult) *presult = result.data;
   if (rc==0 && result.timed_out) rc = UV_ETIMEDOUT;
   if (rc==0 && result.called==0) rc = UV_UNKNOWN;
   return rc;

}
#endif
