/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *   - Laura Schlimmer <laura@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#include <eventql/cli/commands/cluster_set_allocation_policy.h>
#include <eventql/util/cli/flagparser.h>
#include "eventql/config/config_directory.h"
#include "eventql/util/random.h"

namespace eventql {
namespace cli {

const String ClusterSetAllocationPolicy::kName_ = "cluster-set-allocation-policy";
const String ClusterSetAllocationPolicy::kDescription_ =
    "Set allocation policy for a server.";

ClusterSetAllocationPolicy::ClusterSetAllocationPolicy(
    RefPtr<ProcessConfig> process_cfg) :
    process_cfg_(process_cfg) {}

Status ClusterSetAllocationPolicy::execute(
    const std::vector<std::string>& argv,
    FileInputStream* stdin_is,
    OutputStream* stdout_os,
    OutputStream* stderr_os) {
  ::cli::FlagParser flags;
  flags.defineFlag(
      "cluster_name",
      ::cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "node name",
      "<string>");

  flags.defineFlag(
      "server_name",
      ::cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "node name",
      "<string>");

  try {
    flags.parseArgv(argv);

    bool remove_hard = flags.isSet("hard");
    bool remove_soft = flags.isSet("soft");
    if (!(remove_hard ^ remove_soft)) {
      stderr_os->write("ERROR: either --hard or --soft must be set\n");
      return Status(eFlagError);
    }

    ScopedPtr<ConfigDirectory> cdir;
    {
      auto rc = ConfigDirectoryFactory::getConfigDirectoryForClient(
          process_cfg_.get(),
          &cdir);

      if (rc.isSuccess()) {
        rc = cdir->start();
      }

      if (!rc.isSuccess()) {
        stderr_os->write(StringUtil::format("ERROR: $0\n", rc.message()));
        return rc;
      }
    }

    auto cfg = cdir->getServerConfig(flags.getString("server_name"));
    if (remove_soft) {
      cfg.set_is_leaving(true);
    }
    if (remove_hard) {
      cfg.set_is_dead(true);
    }

    cdir->updateServerConfig(cfg);
    cdir->stop();

  } catch (const Exception& e) {
    stderr_os->write(StringUtil::format(
        "$0: $1\n",
        e.getTypeName(),
        e.getMessage()));
    return Status(e);
  }

  return Status::success();
}

const String& ClusterSetAllocationPolicy::getName() const {
  return kName_;
}

const String& ClusterSetAllocationPolicy::getDescription() const {
  return kDescription_;
}

void ClusterSetAllocationPolicy::printHelp(OutputStream* stdout_os) const {
  stdout_os->write(StringUtil::format(
      "\nevqlctl-$0 - $1\n\n", kName_, kDescription_));

  stdout_os->write(
      "Usage: evqlctl [OPTIONS]\n"
      "  --cluster_name <node name>       The name of the cluster to add the server to.\n"
      "  --server_name <server name>      The name of the server to add.\n");
}

} // namespace cli
} // namespace eventql

