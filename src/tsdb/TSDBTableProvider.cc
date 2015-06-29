/**
 * This file is part of the "libfnord" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <fnord/SHA1.h>
#include <tsdb/TSDBTableProvider.h>

using namespace fnord;

namespace tsdb {

TSDBTableProvider::TSDBTableProvider() {}

Option<ScopedPtr<csql::TableExpression>> TSDBTableProvider::buildSequentialScan(
      RefPtr<csql::SequentialScanNode> node,
      csql::Runtime* runtime) const {
  auto table_name = node->tableName();
  static const String prefix = "tsdb://";

  if (!StringUtil::beginsWith(table_name, prefix)) {
    return None<ScopedPtr<csql::TableExpression>>();
  }

  auto path = table_name.substr(prefix.size());
  auto parts = StringUtil::split(path, "/");

  if (parts.size() != 2) {
    RAISEF(
        kIllegalArgumentError,
        "invalid tsdb table reference '$0', format is: "
        "tsdb://<stream>/<partition>",
        table_name);
  }

  auto stream_key = URI::urlDecode(parts[0]);
  auto partition_key = SHA1Hash::fromHexString(URI::urlDecode(parts[1]));

  fnord::iputs("lookup table: $0 / $1", stream_key, partition_key.toString());

  RAISE(kNotImplementedError);
}

} // namespace csql
