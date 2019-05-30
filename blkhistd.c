#include <libudev.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#include "db.h"
#include "udev.h"

int main() {
  sqlite3 *db = NULL;
  struct udev *udev;
  int exit_code = 0;
  int sqlite_rc = 0;

  if ((exit_code = init_udev(&udev)) != UDEV_OK) {
    fprintf(stderr, "udev init failed\n");
    exit(exit_code);
  }

  if ((exit_code = init_db(&db, &sqlite_rc)) != DB_OK) {
    fprintf(stderr, "DB init failed\n");
    exit(exit_code);
  }

  if ((exit_code = init_database_rows(db, udev)) != UDEV_OK) {
    fprintf(stderr, "DB init failed\n");
    exit(exit_code);
  };

  monitor_events(db, udev);

  exit(exit_code);
}