
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db.h"

/* for ntohl/htonl */
#include <netinet/in.h>
#include <arpa/inet.h>

#define MACHINE_ID_LEN 33

#define TRACKER_DBUSER_NAME "TRACKER_DBUSER_NAME"
#define TRACKER_DBUSER_PASSWORD "TRACKER_DBUSER_PASSWORD"
#define TRACKER_HOSTNAME "TRACKER_HOSTNAME"
#define TRACKER_DBNAME "TRACKER_DBNAME"

int init_db(PGconn **conn, char *user, char *password, char *host, char *dbname)
{
  char *connect_str;

  if (conn == NULL)
  {
    return DB_CREATE_ERROR;
  }
  //"user=blockhist password=blockhist host=megadeth-vm-02.storage.lab.eng.bos.redhat.com dbname=block_history"
  int size = asprintf(&connect_str, "user=%s password=%s host=%s dbname=%s",
                      user,
                      password, host, dbname);

  if (size == 0)
  {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  *conn = PQconnectdb("user=blockhist password=blockhist host=megadeth-vm-02.storage.lab.eng.bos.redhat.com dbname=block_history");

  if (PQstatus(*conn) == CONNECTION_BAD)
  {
    fprintf(stderr, "PQconnectdb %s\n", (const char *)PQerrorMessage(*conn));
    return DB_CREATE_ERROR;
  }

  int lib_ver = PQlibVersion();

  printf("Version of libpq: %d\n", lib_ver);

  PGresult *res = PQexec(*conn, "create table if not exists systems (\
                         id serial PRIMARY KEY UNIQUE NOT NULL,\
                         hostname text NOT NULL,\
                         machine_id text UNIQUE NOT NULL,\
                         arch text NOT NULL,\
                         fqdn text NOT NULL,\
                         time_stamp timestamp NOT NULL DEFAULT NOW())");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "PQexec create table systems %s\n", (const char *)PQerrorMessage(*conn));
    return DB_CREATE_ERROR;
  }
  PQclear(res);

  res = PQexec(*conn, "create table if not exists device_history ( \
                            device_id serial PRIMARY KEY UNIQUE NOT NULL,\
                            system_id integer REFERENCES systems(id),\
                            syspath varchar(256),\
                            devpath varchar(256),\
                            subsystem varchar(32),\
                            devtype varchar(32),\
                            devnode varchar(256),\
                            wwn varchar(32),\
                            serial_num varchar(32),\
                            major int,\
                            minor int,\
                            blkdev_size bigint,\
                            time_stamp timestamp NOT NULL DEFAULT NOW())");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "PQexec %s\n", (const char *)PQerrorMessage(*conn));
    return DB_CREATE_ERROR;
  }
  PQclear(res);
  res = PQexec(*conn, "create table if not exists events ( \
                event_id bigserial NOT NULL,\
                device_id uuid NOT NULL,\
                type text NOT NULL,\
                log_date timestamptz NOT NULL DEFAULT now(),\
                PRIMARY KEY (event_id),\
                FOREIGN KEY (device_id)\
                     REFERENCES device_history (device_id))");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "PQexec %s\n", (const char *)PQerrorMessage(*conn));
    return DB_CREATE_ERROR;
  }
  PQclear(res);

  return DB_OK;
}

int close_db(PGconn *conn)
{
  PQfinish(conn);

  return DB_OK;
}

int get_dbconn_data(char **user, char **password, char **host, char **dbname)
{
  if ((*user = getenv(TRACKER_DBUSER_NAME)) == NULL)
  {
    fprintf(stderr, "Error TRACKER_DBUSER_NAME not set\n");
    return DB_CONNECTION_ERROR;
  }
  if ((*password = getenv(TRACKER_DBUSER_PASSWORD)) == NULL)
  {
    fprintf(stderr, "Error TRACKER_DBUSER_PASSWORD not set\n");
    return DB_CONNECTION_ERROR;
  }
  if ((*host = getenv(TRACKER_HOSTNAME)) == NULL)
  {
    fprintf(stderr, "Error TRACKER_HOSTNAME not set\n");
    return DB_CONNECTION_ERROR;
  }
  if ((*dbname = getenv(TRACKER_DBNAME)) == NULL)
  {
    fprintf(stderr, "Error TRACKER_DBNAME not set\n");
    return DB_CONNECTION_ERROR;
  }
  return DB_OK;
}

void printPGresult(PGresult *res)
{
  int i, j;
  printf("printPGresult: %d tuples, %d fields\n", PQntuples(res), PQnfields(res));

  /* print column name */
  for (i = 0; i < PQnfields(res); i++)
    printf("%s\t", PQfname(res, i));

  printf("\n");

  /* print column values */
  for (i = 0; i < PQntuples(res); i++)
  {
    for (j = 0; j < PQnfields(res); j++)
      printf("%s\t", PQgetvalue(res, i, j));
    printf("\n");
  }
}

int get_system_id(PGconn *conn, char *hostname, char *machine_id, char *arch, char *fqdn, int *system_id)
{
  char *sql;
  int size = asprintf(&sql, "select id from systems \
                 where\
                        hostname = '%s' and machine_id = '%s' and arch = '%s' and fqdn = '%s';",
                      hostname, machine_id, arch, fqdn);

  if (size == 0)
  {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  PGresult *res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {

    fprintf(stderr, "select id from systems %s\n", (const char *)PQerrorMessage(conn));
    return DB_NOT_FOUND;
  }

  if (PQntuples(res) == 0)
  {
    return DB_NOT_FOUND;
  }

  printPGresult(res);
  *system_id = atoi(PQgetvalue(res, 0, 0));
  printf("system_id = %d\n", *system_id);
  return DB_OK;
}

int add_system(PGconn *conn, int *system_id)
{
  int size = 0;
  char *sql;
  char hostname[HOST_NAME_MAX + 1];
  char fqdn[_POSIX_HOST_NAME_MAX + 1];
  char machine_id[MACHINE_ID_LEN];
  char arch[32];

  gethostname(hostname, HOST_NAME_MAX + 1);

  FILE *file = popen("hostname --fqdn", "r");
  fgets(fqdn, MACHINE_ID_LEN, file);
  fqdn[strlen(fqdn) - 1] = '\0';
  fclose(file);

  file = popen("cat /etc/machine-id", "r");
  fgets(machine_id, MACHINE_ID_LEN, file);
  fclose(file);

  file = popen("arch", "r");
  fgets(arch, 32, file);
  fclose(file);

  printf("hostname = %s\n fqdn = %s\nmachine_id = %s\n arch = %s\n", hostname, fqdn, machine_id, arch);

  if (get_system_id(conn, hostname, machine_id, arch, fqdn, system_id) == DB_NOT_FOUND)
  {

    size = asprintf(&sql, "insert into systems \
          (hostname, machine_id, arch, fqdn) \
          values('%s', '%s', '%s', '%s') ON CONFLICT\
          (machine_id) DO NOTHING;",
                    hostname, machine_id, arch, fqdn);

    if (size == 0)
    {
      fprintf(stderr, "Error in asprintf\n");
      return DB_FORMAT_ERROR;
    }
    PGresult *res = PQexec(conn, sql);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "insert into events %s\n", (const char *)PQerrorMessage(conn));
      return DB_FORMAT_ERROR;
    }

    PQclear(res);

    get_system_id(conn, hostname, machine_id, arch, fqdn, system_id);
  }

  printf("system_id = %d\n", *system_id);
  return DB_OK;
}

int add_event(PGconn *conn, const char *event, int device_id)
{
  char *sql;
  int size = 0;

  printf("Add event %s device id %d\n", event, device_id);
  /* Add "discovered" event */
  size = asprintf(&sql, "insert into events \
        (dev_history_id, event) \
        values(%d, '%s')",
                  device_id, event);

  if (size == 0)
  {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  PGresult *res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "insert into events %s\n", (const char *)PQerrorMessage(conn));
    return DB_FORMAT_ERROR;
  }

  PQclear(res);

  return DB_OK;
}

int get_device_id(PGconn *conn, const char *syspath,
                  const char *devpath, const char *subsystem, const char *devtype,
                  const char *devnode, const char *wwn, const char *serial,
                  int major, int minor, uint64_t blkdev_size, int *device_id)
{
  char *sql;
  int size = asprintf(&sql, "select device_id from device_history \
                 where\
                        syspath = '%s' and devpath = '%s' and subsystem = '%s' and devtype = '%s' and \
                            devnode = '%s' and wwn = '%s' and serial_num = '%s' and major = '%d' and minor = '%d' and blkdev_size = '%ld';",
                      syspath, devpath, subsystem, devtype, devnode, wwn, serial, major, minor, blkdev_size);

  if (size == 0)
  {
    fprintf(stderr, "Error in asprintf\n");
    return DB_FORMAT_ERROR;
  }

  PGresult *res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {

    fprintf(stderr, "select id from systems %s\n", (const char *)PQerrorMessage(conn));
    return DB_NOT_FOUND;
  }

  if (PQntuples(res) == 0)
  {
    return DB_NOT_FOUND;
  }

  printPGresult(res);
  *device_id = atoi(PQgetvalue(res, 0, 0));
  printf("device_id = %d\n", *device_id);
  return DB_OK;
}

int update_row(PGconn *conn, int system_id, const char *event, const char *syspath,
               const char *devpath, const char *subsystem, const char *devtype,
               const char *devnode, const char *wwn, const char *serial,
               int major, int minor, uint64_t blkdev_size)
{
  char *sql;
  int size = 0;
  int rc = 0;
  int device_id;

  if (get_device_id(conn, syspath, devpath, subsystem, devtype, devnode, wwn, serial, major, minor, blkdev_size, &device_id) == DB_NOT_FOUND)
  {

    printf("Adding new %s \n", devnode);
    size = asprintf(&sql, "insert into device_history \
            (system_id, syspath, devpath, subsystem, devtype, \
             devnode, wwn, serial_num, major, minor, \
             blkdev_size) \
            values('%d', '%s', '%s', '%s','%s','%s','%s','%s', %d, %d, %lu)",
                    system_id, syspath, devpath, subsystem, devtype, devnode, wwn, serial,
                    major, minor, blkdev_size);

    if (size == 0)
    {
      fprintf(stderr, "Error in asprintf\n");
      return DB_FORMAT_ERROR;
    }
    PGresult *res = PQexec(conn, sql);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "insert into device_history %s\n", (const char *)PQerrorMessage(conn));
      return DB_FORMAT_ERROR;
    }

    PQclear(res);
    get_device_id(conn, syspath, devpath, subsystem, devtype, devnode, wwn, serial, major, minor, blkdev_size, &device_id);
  }

  rc = add_event(conn, "discovered", device_id);

  if (rc != DB_OK)
  {
    return rc;
  }

  return DB_OK;
}