#include "wyhash.h"
#include <sys/mman.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <assert.h>

static uint64_t clock_gettime_nsec(clockid_t);

#define MEM_SIZE ((uint64_t)1024*1024*1024*1 / sizeof(uint64_t))
#define TARGET_TEST_TIME 20.0

uint64_t *from_area=NULL, *to_area=NULL;


void bench_write_naive() {
  for(uint64_t i=0; i < MEM_SIZE; i++) {
    to_area[i] = i;
  }
}

void bench_write_nontemporal() {
  for(uint64_t i=0; i < MEM_SIZE; i++) {
    __builtin_nontemporal_store(i,to_area+i);
  }
}

void bench_write_random() {
  uint64_t seed = 0;
  for(uint64_t i=0; i < MEM_SIZE/1; i++) {
    // uint64_t at = (wyrand(&seed) % (MEM_SIZE/8))<<3;
    uint64_t at = (wyrand(&seed) % (MEM_SIZE));
    to_area[at] = i;
    // to_area[at+1] = i+1;
    // to_area[at+2] = i+2;
    // to_area[at+3] = i+3;
    // to_area[at+4] = i+4;
    // to_area[at+5] = i+5;
    // to_area[at+6] = i+6;
    // to_area[at+7] = i+7;
  }
}

void bench_write_random_nontemporal() {
  uint64_t seed = 0;
  for(uint64_t i=0; i < MEM_SIZE/1; i++) {
    // uint64_t at = (wyrand(&seed) % (MEM_SIZE/8))<<3;
    uint64_t at = (wyrand(&seed) % (MEM_SIZE));
    __builtin_nontemporal_store(i,to_area+at);
    // __builtin_nontemporal_store(i+1,to_area+at+1);
    // __builtin_nontemporal_store(i+2,to_area+at+2);
    // __builtin_nontemporal_store(i+3,to_area+at+3);
    // __builtin_nontemporal_store(i+4,to_area+at+4);
    // __builtin_nontemporal_store(i+5,to_area+at+5);
    // __builtin_nontemporal_store(i+6,to_area+at+6);
    // __builtin_nontemporal_store(i+7,to_area+at+7);
  }
}

void bench_copy_naive() {
  for(uint64_t i=0; i < MEM_SIZE; i++) {
    to_area[i] = from_area[i];
    from_area[i] = i;
  }
}

void bench_copy_nontemporal() {
  for(uint64_t i=0; i < MEM_SIZE; i++) {
    __builtin_nontemporal_store(from_area[i],to_area+i);
    from_area[i] = i;
  }
}

void __attribute__ ((noinline)) bench_copy_random(uint64_t block_size) {
  uint64_t seed = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID);
  for(uint64_t i=0; i < MEM_SIZE; i+=block_size) {
    uint64_t at = (wyrand(&seed) % (MEM_SIZE-block_size));
    for(uint64_t n=0;n<block_size;n++)
      to_area[i+n] = from_area[at+n];
    from_area[at] = i;
  }
}

void __attribute__ ((noinline)) bench_copy_random_opt(uint64_t block_size) {
  uint64_t seed = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID);
  for(uint64_t i=0; i < MEM_SIZE; i+=block_size) {
    uint64_t at = (wyrand(&seed) % (MEM_SIZE-block_size));
    switch(block_size) {
      case 1:
        to_area[i+0] = from_area[at+0];
        break;
      case 2:
        to_area[i+0] = from_area[at+0];
        to_area[i+1] = from_area[at+1];
        break;
      case 3:
        to_area[i+0] = from_area[at+0];
        to_area[i+1] = from_area[at+1];
        to_area[i+2] = from_area[at+2];
        break;
      case 4:
        to_area[i+0] = from_area[at+0];
        to_area[i+1] = from_area[at+1];
        to_area[i+2] = from_area[at+2];
        to_area[i+3] = from_area[at+3];
        break;
      default:
        for(uint64_t n=0;n<block_size;n++)
          to_area[i+n] = from_area[at+n];
        break;
    }
    from_area[at] = i;
  }
}

void bench_copy_random_nontemporal() {
  uint64_t seed = 0;
  for(uint64_t i=0; i < MEM_SIZE; i+=2) {
    uint64_t at1 = (wyrand(&seed) % (MEM_SIZE));
    uint64_t at2 = (wyrand(&seed) % (MEM_SIZE));
    if(at1!=at2) {
      __builtin_nontemporal_store(__builtin_nontemporal_load(from_area+at1),to_area+i+0);
      __builtin_nontemporal_store(__builtin_nontemporal_load(from_area+at2),to_area+i+1);
    }
  }
}










void touchSpace(uint64_t *ptr, uint64_t size) {
  for(int i=0; i < size; i+=512) {
    ptr[i] = 0;
  }
}

static char *pp_speed(uint64_t words, uint64_t time) {
  static int i=0;
  static char buffer[10][128];
  uint64_t rate = words*8/1024 / (time/1e9);
  i = (i+1) % 10;
  if(rate < 1024) {
    snprintf(buffer[i], 128, "%4lu KiB/s", rate);
  } if(rate < 1024*1024*10) { // Less than 10000 MB/s
    snprintf(buffer[i], 128, "%4lu MiB/s", rate/1024);
  } else {
    snprintf(buffer[i], 128, "%4.1f GiB/s", (double)rate/1024/1024);
  }
  return buffer[i];
}

static uint64_t clock_gettime_nsec(clockid_t clk_id) {
  struct timespec t;
  clock_gettime(clk_id,&t);
  return (uint64_t)t.tv_sec*1e9 + t.tv_nsec;
}

#define RUN_BENCH(fn, msg) \
  { \
  int i=0; \
  start = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID); \
  for(;;) { \
    i++; fn; \
    total = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID) - start; \
    if( total/1e9 > TARGET_TEST_TIME) break; \
  } \
  printf("%s: %s\n", msg, pp_speed(MEM_SIZE*i, total)); \
  }

int main(int argc, char* argv[]) {
  from_area = mmap(NULL, MEM_SIZE*sizeof(uint64_t), PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  to_area = mmap(NULL, MEM_SIZE*sizeof(uint64_t), PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  assert(from_area != NULL);
  assert(to_area != NULL);
  uint64_t start = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID);
  touchSpace(from_area, MEM_SIZE);
  touchSpace(to_area, MEM_SIZE);
  uint64_t total = clock_gettime_nsec(CLOCK_PROCESS_CPUTIME_ID) - start;
  printf("Zeroed pages: %s\n", pp_speed(MEM_SIZE+MEM_SIZE, total));

  for(int i=1; i<argc;i++) {
    if(strcmp(argv[i],"write_naive")==0) {
      RUN_BENCH(bench_write_naive(), "Write naive");
    } else if(strcmp(argv[i],"write_nontemporal")==0) {
      RUN_BENCH(bench_write_nontemporal(), "Write nontemporal");
    } else if(strcmp(argv[i],"write_random")==0) {
      RUN_BENCH(bench_write_random(), "Write random");
    } else if(strcmp(argv[i],"write_random_nontemporal")==0) {
      RUN_BENCH(bench_write_random_nontemporal(), "Write random nontemporal");
    } else if(strcmp(argv[i],"copy_naive")==0) {
      RUN_BENCH(bench_copy_naive(), "Copy naive");
    } else if(strcmp(argv[i],"copy_nontemporal")==0) {
      RUN_BENCH(bench_copy_nontemporal(), "Copy nontempral");
    } else if(strcmp(argv[i],"copy_random")==0) {
      RUN_BENCH(bench_copy_random(1), "Copy random");
    } else if(strcmp(argv[i],"copy_random_opt")==0) {
      RUN_BENCH(bench_copy_random_opt(1), "Copy random (opt)");
    } else if(strcmp(argv[i],"copy_random_table")==0) {
      for(uint64_t opt=0;opt<16;opt++) {
        char msg[1024];
        sprintf(msg, "Copy random %d", 1<<opt);
        RUN_BENCH(bench_copy_random((1<<opt)), msg);
      }
    } else if(strcmp(argv[i],"copy_random_nontempral")==0) {
      RUN_BENCH(bench_copy_random_nontemporal(), "Copy random nontemporal");
    } else {
      printf("Unknown action: %s\n", argv[i]);
      return 1;
    }
  }
  return 0;
}