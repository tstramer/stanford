/** 
    On certain Ubuntu machines (such as the myth cluster), certain functions in libc can rely on libpthread
    but it doesn't link against it or even let you link against it unless your code
    calls pthread. So this is a workaround. The true cause is being investigated, but this should allow you to 
    still build your programs.
    http://stackoverflow.com/questions/31579243/segmentation-fault-before-main-when-using-glut-and-stdstring
*/
#if defined(__linux__)
#include <pthread.h>

void* simpleFunc(void*) {
  return NULL;
}

void forcePThreadLink() {
  pthread_t t1;
  pthread_create(&t1, NULL, &simpleFunc, NULL);
}
#endif
