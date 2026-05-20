#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Compile with `-pthread`

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;

void close_logger(void) {
  pthread_mutex_lock(&log_mutex);
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }
  pthread_mutex_unlock(&log_mutex);
}

void __log_indir(const char *src_info, uintptr_t dst_addr) {
  pthread_mutex_lock(&log_mutex);

  if (!log_file) {
    log_file = fopen("indir_log.txt", "a");
    if (log_file)
      atexit(close_logger);
  }

  if (log_file) {
    fprintf(log_file, "Source: %s -> Dest: 0x%lx\n", src_info,
            (unsigned long)dst_addr);
    fflush(log_file);
  }

  pthread_mutex_unlock(&log_mutex);
}
