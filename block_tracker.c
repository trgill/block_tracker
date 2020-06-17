#include <libudev.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#include "db.h"
#include "udev.h"

int main()
{
  PGconn *conn = NULL;
  int system_id;
  char *user, *password, *host, *dbname;

  struct udev *udev;
  int exit_code = 0;

  if ((exit_code = get_dbconn_data(&user, &password, &host, &dbname)) != UDEV_OK)
  {
    fprintf(stderr, "get_dbconn_data failed\n");
    exit(exit_code);
  }

  if ((exit_code = init_udev(&udev)) != UDEV_OK)
  {
    fprintf(stderr, "udev init failed\n");
    exit(exit_code);
  }

  if ((exit_code = init_db(&conn, user, password, host, dbname)) != DB_OK)
  {
    fprintf(stderr, "DB init failed\n");
    exit(exit_code);
  }

  if ((exit_code = add_system(conn, &system_id)) != DB_OK)
  {
    fprintf(stderr, "add_system failed\n");
    exit(exit_code);
  }

  if ((exit_code = init_database_rows(conn, system_id, udev)) != UDEV_OK)
  {
    fprintf(stderr, "DB init failed\n");
    exit(exit_code);
  };

  monitor_events(conn, udev, system_id);

  if ((exit_code = close_db(conn)) != UDEV_OK)
  {
    fprintf(stderr, "close_db failed\n");
    exit(exit_code);
  }
  exit(exit_code);
}