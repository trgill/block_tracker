#include <sqlite3.h>
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

int init_db(sqlite3 **db, int *sqlite_rc) {
  int rc;
  char *err_msg = NULL;

  if (db == NULL) {
    return DB_CREATE_ERROR;
  }

  rc = sqlite3_open(DB_NAME, db);

  if (rc) {
    fprintf(stderr, "sqlite3_open %s\n", sqlite3_errmsg(*db));
    return DB_CREATE_ERROR;
  }

  char *pSQL = "create table if not exists dev_history ( \
                id INTEGER PRIMARY KEY, \
                syspath varchar(256), \
                devpath varchar(256), \
                subsystem varchar(32), \
                devtype varchar(32), \
                devnode varchar(256), \
                wwn varchar(32), \
                serial varchar (32), \
                major int, \
                minor int, \
                blkdev_size bigint unsigned, \
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";

  rc = sqlite3_exec(*db, pSQL, 0, 0, &err_msg);

  if (rc) {
    fprintf(stderr, "sqlite3_exec: dev_history %s\n", sqlite3_errmsg(*db));
    return DB_CREATE_ERROR;
  }

  pSQL = "create table if not exists events ( \
                dev_history_id bigserial NOT NULL REFERENCES dev_history ON DELETE RESTRICT, \
                event varchar(16), \
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";

  rc = sqlite3_exec(*db, pSQL, 0, 0, &err_msg);

  if (rc) {
    fprintf(stderr, "sqlite3_exec: events %s\n", sqlite3_errmsg(*db));
    return DB_CREATE_ERROR;
  }

  return DB_OK;
}

int add_event(sqlite3 *db, const char *event, const char *devnode) {
  char *sql;
  int size = 0;
  int rc;
  char *err_msg = NULL;

  /* Add "discovered" event */
  size = asprintf(&sql, "insert into events \
        (dev_history_id, event) \
        values((SELECT id from dev_history WHERE devnode='%s'), '%s')",
                  devnode, event);

  if (size == 0) {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  free(sql);

  if (rc) {
    fprintf(stderr, "update_row: sqlite3_exec %s\n", sqlite3_errmsg(db));
    return DB_CREATE_ERROR;
  }

  return DB_OK;
}

int update_row(sqlite3 *db, const char *event, const char *syspath,
               const char *devpath, const char *subsystem, const char *devtype,
               const char *devnode, const char *wwn, const char *serial,
               int major, int minor, uint64_t blkdev_size) {
  sqlite3_stmt *res;
  char *sql;
  int size = 0;
  char *err_msg = NULL;
  bool exists = false;

  size = asprintf(&sql,
                  "select syspath, devpath, subsystem, devtype, devnode, wwn, "
                  "serial, major, minor, blkdev_size timestamp from "
                  "dev_history where devnode = '%s'",
                  devnode);

  if (size == 0) {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc) {
    fprintf(stderr, "update_row: sqlite3_prepare_v2 %s\n", sqlite3_errmsg(db));
    return DB_QUERY_ERROR;
  }
  free(sql);
  sql = NULL;

  while (sqlite3_step(res) == SQLITE_ROW) {
    printf("Search for : %s \n", sqlite3_column_text(res, 4));
    if (strcmp(syspath, (char *)sqlite3_column_text(res, 0)) == 0 &&
        strcmp(devpath, (char *)sqlite3_column_text(res, 1)) == 0 &&
        strcmp(subsystem, (char *)sqlite3_column_text(res, 2)) == 0 &&
        strcmp(devtype, (char *)sqlite3_column_text(res, 3)) == 0 &&
        strcmp(devnode, (char *)sqlite3_column_text(res, 4)) == 0 &&
        strcmp(wwn, (char *)sqlite3_column_text(res, 5)) == 0 &&
        strcmp(serial, (char *)sqlite3_column_text(res, 6)) == 0 &&
        major == sqlite3_column_int(res, 7) &&
        minor == sqlite3_column_int(res, 8)) {
      /* TODO check the size of the device for change */
      printf("Exits = true %s = %s ", devnode, sqlite3_column_text(res, 4));
      exists = true;
    }
  }

  if (!exists) {
    printf("Adding new %s", devnode);
    size = asprintf(&sql, "insert into dev_history \
            (syspath, devpath, subsystem, devtype, devnode, wwn, serial, major, minor, blkdev_size) \
            values('%s', '%s', '%s','%s','%s','%s','%s', %d, %d, %lu)",
                    syspath, devpath, subsystem, devtype, devnode, wwn, serial,
                    major, minor, blkdev_size);

    if (size == 0) {
      fprintf(stderr, "Error in asprintf\n");
      return DB_FORMAT_ERROR;
    }

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    free(sql);

    if (rc) {
      fprintf(stderr, "update_row: sqlite3_exec %s\n", sqlite3_errmsg(db));
      return DB_CREATE_ERROR;
    }

    rc = add_event(db, "discovered", devnode);

    if (rc != DB_OK) {
      return rc;
    }
  } else {
    rc = add_event(db, event, devnode);

    if (rc != DB_OK) {
      return rc;
    }
  }
  sqlite3_finalize(res);

  return DB_OK;
}