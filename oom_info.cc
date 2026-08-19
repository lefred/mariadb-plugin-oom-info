/* Copyright (c) 2019,2024,2025,2026 MariaDB Corporation
   Copyright (c) 2026 lefred (Frédéric Descamps)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

#include <my_global.h>
#include <sql_class.h>
#include <sql_i_s.h>
#include <mysql/plugin.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace {

struct Proc_value
{
  long value;
  bool available;
};

/*
  procfs values are short, but use a fixed upper bound anyway.  This avoids
  allocations based on kernel-controlled input and rejects truncated data.
*/
static Proc_value read_proc_integer(const char *path, long minimum,
                                    long maximum)
{
  char buffer[64];
  int flags= O_RDONLY;
#ifdef O_CLOEXEC
  flags|= O_CLOEXEC;
#endif
  int fd;
  do
  {
    fd= open(path, flags);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0)
    return {0, false};

  ssize_t length;
  do
  {
    length= read(fd, buffer, sizeof(buffer) - 1);
  } while (length < 0 && errno == EINTR);

  char extra;
  ssize_t extra_length= 0;
  if (length == static_cast<ssize_t>(sizeof(buffer) - 1))
  {
    do
    {
      extra_length= read(fd, &extra, 1);
    } while (extra_length < 0 && errno == EINTR);
  }
  close(fd);

  if (length <= 0 || extra_length != 0)
    return {0, false};

  buffer[length]= '\0';
  char *end;
  errno= 0;
  long value= strtol(buffer, &end, 10);
  if (errno == ERANGE || end == buffer || value < minimum || value > maximum)
    return {0, false};

  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
    ++end;
  if (*end != '\0')
    return {0, false};

  return {value, true};
}

static void store_value(Field *field, const Proc_value &value)
{
  if (!value.available)
  {
    field->set_null();
    return;
  }
  field->set_notnull();
  field->store(static_cast<longlong>(value.value), false);
}

static int oom_info_fill_table(THD *thd, TABLE_LIST *tables, Item *)
{
  TABLE *table= tables->table;
  const Proc_value score=
      read_proc_integer("/proc/self/oom_score", 0, 1000);
  const Proc_value adj=
      read_proc_integer("/proc/self/oom_adj", -17, 15);
  const Proc_value score_adj=
      read_proc_integer("/proc/self/oom_score_adj", -1000, 1000);

  store_value(table->field[0], score);
  store_value(table->field[1], adj);
  store_value(table->field[2], score_adj);

  return schema_table_store_record(thd, table) ? 1 : 0;
}

ST_FIELD_INFO oom_info_fields[]=
{
  // Current Linux OOM badness score
  Show::Column("SCORE",     Show::SLonglong(4), NULLABLE),
  // Legacy Linux OOM adjustment
  Show::Column("ADJ",       Show::SLonglong(4), NULLABLE),
  // Linux OOM score adjustment
  Show::Column("SCORE_ADJ", Show::SLonglong(4), NULLABLE),
  Show::CEnd()
};

static int oom_info_init(void *plugin)
{
  ST_SCHEMA_TABLE *schema= static_cast<ST_SCHEMA_TABLE *>(plugin);
  schema->fields_info= oom_info_fields;
  schema->fill_table= oom_info_fill_table;
  return 0;
}

static struct st_mysql_information_schema oom_info_descriptor=
{
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

} // namespace

maria_declare_plugin(oom_info)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &oom_info_descriptor,
  "OOM_INFO",
  "lefred",
  "Linux out-of-memory killer information for the MariaDB server process",
  PLUGIN_LICENSE_GPL,
  oom_info_init,
  nullptr,
  0x0100,
  nullptr,
  nullptr,
  "0.2.0",
  MariaDB_PLUGIN_MATURITY_BETA
}
maria_declare_plugin_end;
