#include <am.h>
#include <klib.h>
#include <rtthread.h>
#include ARCH_H

typedef struct temp_data {
    Context **from;
    Context **to;
} temp_data;

static Context* ev_handler(Event e, Context *c) {
  
  switch (e.event) {
    case EVENT_YIELD: {
      rt_thread_t current_thread = rt_thread_self();
      temp_data *data = (temp_data *)(current_thread->user_data);
      if (data->from) {
        *(Context **)data->from = c;
      }
      if (!data->to) assert(0);

      return *(Context **)data->to;
    }
    case EVENT_IRQ_IODEV:
    case EVENT_IRQ_TIMER: {
      return c;
    }
    default: printf("Unhandled event ID = %d\n", e.event); assert(0);
  }
  return c;
}

void __am_cte_init() {
  cte_init(ev_handler);
}

void rt_hw_context_switch_to(rt_ubase_t to) {
  rt_thread_t current_thread = rt_thread_self();
  rt_ubase_t user_data =  current_thread->user_data;
  temp_data data = (temp_data){.from = (Context **)NULL, .to = (Context **)to};
  current_thread->user_data = (rt_ubase_t)(&data);
  yield();
  current_thread->user_data = user_data;
}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to) {
  rt_thread_t current_thread = rt_thread_self();
  rt_ubase_t user_data =  current_thread->user_data;
  temp_data data = (temp_data){.from = (Context **)from, .to = (Context **)to};
  current_thread->user_data = (rt_ubase_t)(&data);
  yield();
  current_thread->user_data = user_data;
}

void rt_hw_context_switch_interrupt(void *context, rt_ubase_t from, rt_ubase_t to, struct rt_thread *to_thread) {
  assert(0);
}


typedef struct _parameter{
    uintptr_t entry;
    uintptr_t para;
    uintptr_t exit;
} temp_para;

#ifndef __ISA_NATIVE__
void wrapper(void *tentry, void *parameter, void *texit) {
  void (*entry)(void *) = (void (*)(void *)) tentry;
  void (*exit)(void) = (void (*)(void)) texit;

  entry(parameter);
  exit();
}

#else

void wrapper(void *parameter) {
  temp_para *para = parameter;
  void (*entry)(void *) = (void (*)(void *))para->entry;
  void *true_para = (void *)para->para;
  void (*exit)(void) = (void (*)(void))para->exit;
  entry(true_para);
  exit();
}



#endif
#define CONTEXT_SIZE  ((NR_REGS + 3) * 4)
rt_uint8_t *rt_hw_stack_init(void *tentry, void *parameter, rt_uint8_t *stack_addr, void *texit) {
#ifndef __ISA_NATIVE__
  stack_addr = (rt_uint8_t *)((uintptr_t)stack_addr & ~(uintptr_t)((sizeof(uintptr_t) - 1)));  // 向下对齐
  Context *context = (Context *)(stack_addr - CONTEXT_SIZE);

  for (int i = 0; i < NR_REGS; i++) {
    context->gpr[i] = 0;
  }

  context->gpr[10] = (uintptr_t)tentry;
  context->gpr[11] = (uintptr_t)parameter;
  context->gpr[12] = (uintptr_t)texit;

  context->gpr[2] = (uintptr_t)(stack_addr - CONTEXT_SIZE);

  context->mstatus = (uintptr_t)0x1800;
  context->mepc = (uintptr_t)wrapper;

#else
  stack_addr = (rt_uint8_t *)(((uintptr_t)stack_addr) & ~(uintptr_t)(sizeof(uintptr_t) - 1));
  Area kstack = (Area){.start = stack_addr - 1024, .end = stack_addr};
  temp_para *para = (temp_para *)(stack_addr - 256);
  Context *context = kcontext(kstack, (void (*)(void *))wrapper, para);
  para->entry = (uintptr_t)tentry;
  para->para = (uintptr_t)parameter;
  para->exit = (uintptr_t)texit;
#endif

  return (rt_uint8_t *)context;
}