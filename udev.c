#include <libudev.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "db.h"
#include "udev.h"
#include "util.h"

int print_devs_attrs(struct udev_device *device)
{
  if (device == NULL)
  {
    return UDEV_NULL_PARAM;
  }
  const char *str;
  dev_t devnum;
  int count;
  struct udev_list_entry *list_entry;

  printf("*** device: %p ***\n", device);
  str = udev_device_get_action(device);
  if (str != NULL)
    printf("action:    '%s'\n", str);

  str = udev_device_get_syspath(device);
  printf("syspath:   '%s'\n", str);

  str = udev_device_get_sysname(device);
  printf("sysname:   '%s'\n", str);

  str = udev_device_get_sysnum(device);
  if (str != NULL)
    printf("sysnum:    '%s'\n", str);

  str = udev_device_get_devpath(device);
  printf("devpath:   '%s'\n", str);

  str = udev_device_get_subsystem(device);
  if (str != NULL)
    printf("subsystem: '%s'\n", str);

  str = udev_device_get_devtype(device);
  if (str != NULL)
    printf("devtype:   '%s'\n", str);

  str = udev_device_get_driver(device);
  if (str != NULL)
    printf("driver:    '%s'\n", str);

  str = udev_device_get_devnode(device);
  if (str != NULL)
    printf("devnode:   '%s'\n", str);

  devnum = udev_device_get_devnum(device);
  if (major(devnum) > 0)
    printf("devnum:    %u:%u\n", major(devnum), minor(devnum));

  count = 0;
  udev_list_entry_foreach(list_entry,
                          udev_device_get_devlinks_list_entry(device))
  {
    printf("link:      '%s'\n", udev_list_entry_get_name(list_entry));
    count++;
  }
  if (count > 0)
    printf("found %i links\n", count);

  count = 0;
  udev_list_entry_foreach(list_entry,
                          udev_device_get_properties_list_entry(device))
  {
    printf("property:  '%s=%s'\n", udev_list_entry_get_name(list_entry),
           udev_list_entry_get_value(list_entry));
    count++;
  }
  if (count > 0)
    printf("found %i properties\n", count);

  str = udev_device_get_property_value(device, "MAJOR");
  if (str != NULL)
    printf("MAJOR: '%s'\n", str);

  str = udev_device_get_sysattr_value(device, "dev");
  if (str != NULL)
    printf("attr{dev}: '%s'\n", str);

  printf("\n");

  return UDEV_OK;
}

int init_udev(struct udev **udev)
{

  if (udev == NULL)
  {
    return UDEV_NULL_PARAM;
  }

  *udev = udev_new();
  if (!udev)
  {
    fprintf(stderr, "Cannot create udev context.\n");
    return UDEV_INIT_FAILED;
  }

  return UDEV_OK;
}

int init_database_rows(PGconn *conn, int system_id, struct udev *udev)
{
  struct udev_device *device;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  dev_t devnum;

  enumerate = udev_enumerate_new(udev);
  if (!enumerate)
  {
    fprintf(stderr, "Cannot create enumerate context.\n");
    return UDEV_ENUMERATE_CONTEXT_FAILED;
  }

  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);

  devices = udev_enumerate_get_list_entry(enumerate);
  if (!devices)
  {
    fprintf(stderr, "Failed to get device list.\n");
    return UDEV_GET_LIST_FAILED;
  }

  udev_list_entry_foreach(dev_list_entry, devices)
  {
    const char *path;

    path = udev_list_entry_get_name(dev_list_entry);
    device = udev_device_new_from_syspath(udev, path);
    devnum = udev_device_get_devnum(device);

    print_devs_attrs(device);

    if (update_row(
            conn, system_id, NULL, udev_device_get_syspath(device),
            udev_device_get_devpath(device), udev_device_get_subsystem(device),
            udev_device_get_devtype(device), udev_device_get_devnode(device),
            udev_device_get_property_value(device, "ID_WWN_WITH_EXTENSION"),
            udev_device_get_property_value(device, "ID_SERIAL"), major(devnum),
            minor(devnum),
            get_blkdev_size(udev_device_get_devnode(device))) != DB_OK)
    {
      fprintf(stderr, "Database update failed.\n");
    }

    udev_device_unref(device);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return UDEV_OK;
}

int monitor_events(PGconn *conn, struct udev *udev, int system_id)
{
  struct udev_device *device;
  struct udev_monitor *mon;
  int fd;
  dev_t devnum;
  const char *action;
  uint64_t blkdev_size = 0;

  mon = udev_monitor_new_from_netlink(udev, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
  udev_monitor_enable_receiving(mon);
  fd = udev_monitor_get_fd(mon);

  while (1)
  {
    fd_set fds;
    struct timeval tv;
    int ret;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(fd, &fds))
    {
      device = udev_monitor_receive_device(mon);
      if (device)
      {
        print_devs_attrs(device);
        printf("action = %s devnode = %s\n", udev_device_get_action(device),
               udev_device_get_devnode(device));
        devnum = udev_device_get_devnum(device);
        action = udev_device_get_action(device);

        if (strcmp(action, "remove") != 0)
        {
          blkdev_size = get_blkdev_size(udev_device_get_devnode(device));
        }

        if (update_row(
                conn, system_id, action, udev_device_get_syspath(device),
                udev_device_get_devpath(device),
                udev_device_get_subsystem(device),
                udev_device_get_devtype(device),
                udev_device_get_devnode(device),
                udev_device_get_property_value(device, "ID_WWN_WITH_EXTENSION"),
                udev_device_get_property_value(device, "ID_SERIAL"),
                major(devnum), minor(devnum), blkdev_size) != DB_OK)
        {
          fprintf(stderr, "Database update failed.\n");
        }

        udev_device_unref(device);
      }
    }

    sleep(1);
  }
  /* free udev */
  udev_unref(udev);

  return 0;
}