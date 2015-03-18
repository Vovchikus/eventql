/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "fnord-base/io/fileutil.h"
#include "fnord-base/application.h"
#include "fnord-base/logging.h"
#include "fnord-base/Language.h"
#include "fnord-base/cli/flagparser.h"
#include "fnord-base/util/SimpleRateLimit.h"
#include "fnord-base/InternMap.h"
#include "fnord-json/json.h"
#include "fnord-mdb/MDB.h"
#include "fnord-mdb/MDBUtil.h"
#include "fnord-sstable/sstablereader.h"
#include "fnord-sstable/sstablewriter.h"
#include "fnord-sstable/SSTableColumnSchema.h"
#include "fnord-sstable/SSTableColumnReader.h"
#include "fnord-sstable/SSTableColumnWriter.h"
#include "common.h"
#include "CustomerNamespace.h"
#include "FeatureSchema.h"
#include "JoinedQuery.h"
#include "CTRCounter.h"
#include "Analyzer.h"

using namespace fnord;
using namespace cm;

struct ItemStats {
  ItemStats() : views(0), clicks(0) {}
  uint32_t views;
  uint32_t clicks;
  HashMap<void*, uint32_t> term_counts;
};

struct GlobalCounter {
  uint32_t views;
  uint32_t clicks;
};

typedef HashMap<uint64_t, ItemStats> CounterMap;

InternMap intern_map;

void indexJoinedQuery(
    const cm::JoinedQuery& query,
    ItemEligibility eligibility,
    FeatureIndex* feature_index,
    Analyzer* analyzer,
    Language lang,
    CounterMap* counters,
    GlobalCounter* global_counter) {
  if (!isQueryEligible(eligibility, query)) {
    return;
  }

  /* query string terms */
  auto qstr_opt = cm::extractAttr(query.attrs, "qstr~de"); // FIXPAUL
  if (qstr_opt.isEmpty()) {
    return;
  }

  Set<String> qstr_terms;
  analyzer->extractTerms(lang, qstr_opt.get(), &qstr_terms);

  for (const auto& item : query.items) {
    if (!isItemEligible(eligibility, query, item)) {
      continue;
    }

    auto& stats = (*counters)[std::stoul(item.item.item_id)];
    ++stats.views;
    stats.clicks += (int) item.clicked;

    ++global_counter->views;
    global_counter->clicks += (int) item.clicked;
  }
}

/* write output table */
void writeOutputTable(
    const String& filename,
    const CounterMap& counters,
    uint64_t start_time,
    uint64_t end_time) {
  /* prepare output sstable schema */
  //sstable::SSTableColumnSchema sstable_schema;
  //sstable_schema.addColumn("num_views", 1, sstable::SSTableColumnType::UINT64);
  //sstable_schema.addColumn("num_clicks", 2, sstable::SSTableColumnType::UINT64);
  //sstable_schema.addColumn("num_clicked", 3, sstable::SSTableColumnType::UINT64);

  //HashMap<String, String> out_hdr;
  //out_hdr["start_time"] = StringUtil::toString(start_time);
  //out_hdr["end_time"] = StringUtil::toString(end_time);
  //auto outhdr_json = json::toJSONString(out_hdr);

  ///* open output sstable */
  //fnord::logInfo("cm.ctrstats", "Writing results to: $0", filename);
  //auto sstable_writer = sstable::SSTableWriter::create(
  //    filename,
  //    sstable::IndexProvider{},
  //    outhdr_json.data(),
  //    outhdr_json.length());


  //for (const auto& p : counters) {
  //  sstable::SSTableColumnWriter cols(&sstable_schema);
  //  cols.addUInt64Column(1, p.second.num_views);
  //  cols.addUInt64Column(2, p.second.num_clicks);
  //  cols.addUInt64Column(3, p.second.num_clicked);

  //  String key_str;
  //  switch (p.first.length()) {

  //    case 0: {
  //      key_str = "__GLOBAL";
  //      break;
  //    }

  //    case (sizeof(void*)): {
  //      key_str = intern_map.getString(((void**) p.first.c_str())[0]);
  //      break;
  //    }

  //    case (sizeof(void*) * 2): {
  //      key_str = intern_map.getString(((void**) p.first.c_str())[0]);
  //      key_str += "~";
  //      key_str += intern_map.getString(((void**) p.first.c_str())[1]);
  //      break;
  //    }

  //    default:
  //      RAISE(kRuntimeError, "invalid counter key");

  //  }

  //  sstable_writer->appendRow(key_str, cols);
  //}

  //sstable_schema.writeIndex(sstable_writer.get());
  //sstable_writer->finalize();
}

int main(int argc, const char** argv) {
  fnord::Application::init();
  fnord::Application::logToStderr();

  fnord::cli::FlagParser flags;

  flags.defineFlag(
      "lang",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "language",
      "<lang>");

  flags.defineFlag(
      "conf",
      cli::FlagParser::T_STRING,
      false,
      NULL,
      "./conf",
      "conf directory",
      "<path>");

  flags.defineFlag(
      "output_file",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "output file path",
      "<path>");

  flags.defineFlag(
      "featuredb_path",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "feature db path",
      "<path>");

  flags.defineFlag(
      "loglevel",
      fnord::cli::FlagParser::T_STRING,
      false,
      NULL,
      "INFO",
      "loglevel",
      "<level>");

  flags.parseArgv(argc, argv);

  Logger::get()->setMinimumLogLevel(
      strToLogLevel(flags.getString("loglevel")));

  CounterMap counters;
  GlobalCounter global_counter;
  auto start_time = std::numeric_limits<uint64_t>::max();
  auto end_time = std::numeric_limits<uint64_t>::min();

  auto lang = languageFromString(flags.getString("lang"));
  cm::Analyzer analyzer(flags.getString("conf"));

  /* set up feature schema */
  cm::FeatureSchema feature_schema;
  feature_schema.registerFeature("shop_id", 1, 1);
  feature_schema.registerFeature("category1", 2, 1);
  feature_schema.registerFeature("category2", 3, 1);
  feature_schema.registerFeature("category3", 4, 1);
  feature_schema.registerFeature("title~de", 5, 2);

  /* open featuredb db */
  auto featuredb_path = flags.getString("featuredb_path");
  auto featuredb = mdb::MDB::open(featuredb_path, true);
  cm::FeatureIndex feature_index(featuredb, &feature_schema);

  /* read input tables */
  auto sstables = flags.getArgv();
  auto tbl_cnt = sstables.size();
  for (int tbl_idx = 0; tbl_idx < sstables.size(); ++tbl_idx) {
    const auto& sstable = sstables[tbl_idx];
    fnord::logInfo("cm.ctrstats", "Importing sstable: $0", sstable);

    /* read sstable header */
    sstable::SSTableReader reader(File::openFile(sstable, File::O_READ));

    if (reader.bodySize() == 0) {
      fnord::logCritical("cm.ctrstats", "unfinished sstable: $0", sstable);
      exit(1);
    }

    /* read report header */
    auto hdr = json::parseJSON(reader.readHeader());

    auto tbl_start_time = json::JSONUtil::objectGetUInt64(
        hdr.begin(),
        hdr.end(),
        "start_time").get();

    auto tbl_end_time = json::JSONUtil::objectGetUInt64(
        hdr.begin(),
        hdr.end(),
        "end_time").get();

    if (tbl_start_time < start_time) {
      start_time = tbl_start_time;
    }

    if (tbl_end_time > end_time) {
      end_time = tbl_end_time;
    }

    /* get sstable cursor */
    auto cursor = reader.getCursor();
    auto body_size = reader.bodySize();
    int row_idx = 0;

    /* status line */
    util::SimpleRateLimitedFn status_line(kMicrosPerSecond, [&] () {
      auto p = (tbl_idx / (double) tbl_cnt) +
          ((cursor->position() / (double) body_size)) / (double) tbl_cnt;

      fnord::logInfo(
          "cm.ctrstats",
          "[$0%] Reading sstables... rows=$1",
          (size_t) (p * 100),
          row_idx);
    });

    /* read sstable rows */
    for (; cursor->valid(); ++row_idx) {
      status_line.runMaybe();

      auto val = cursor->getDataBuffer();
      Option<cm::JoinedQuery> q;

      try {
        q = Some(json::fromJSON<cm::JoinedQuery>(val));
      } catch (const Exception& e) {
        //fnord::logWarning("cm.ctrstats", e, "invalid json: $0", val.toString());
      }

      if (!q.isEmpty()) {
        indexJoinedQuery(
            q.get(),
            cm::ItemEligibility::DAWANDA_ALL_NOBOTS,
            &feature_index,
            &analyzer,
            lang,
            &counters,
            &global_counter);
      }

      if (!cursor->next()) {
        break;
      }
    }

    status_line.runForce();
  }

  auto baseline_ctr = global_counter.clicks / (double) global_counter.views;

  fnord::iputs("baseline ctr: $0", baseline_ctr);

  for (const auto& c : counters) {
    auto ctr = c.second.clicks / (double) c.second.views;

    fnord::iputs("id=$0 views=$1 clicks=$2 ctr=$3 perf=$4",
        c.first,
        c.second.views,
        c.second.clicks,
        ctr,
        ctr / baseline_ctr);
  }

  /* write output table */
  //writeOutputTable(
  //    flags.getString("output_file"),
  //    counters,
  //    start_time,
  //    end_time);

  return 0;
}

