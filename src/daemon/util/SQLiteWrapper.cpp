/* Written in 2015 by Alexander Shishenko <alex@shishenko.com>
 *
 * LVSQLite - SQLite wrapper, used in Librevault.
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include "SQLiteWrapper.h"

#include "util/blob.h"

namespace librevault {

// SQLValue
SQLValue::SQLValue() : value_type(ValueType::NULL_VALUE) {}
SQLValue::SQLValue(int64_t int_val) : value_type(ValueType::INT), value((qlonglong)int_val) {}
SQLValue::SQLValue(uint64_t int_val) : value_type(ValueType::INT), value((qulonglong)int_val) {}
SQLValue::SQLValue(double double_val) : value_type(ValueType::DOUBLE), value(double_val) {}

SQLValue::SQLValue(const std::string& text_val)
    : value_type(ValueType::TEXT), value(QString::fromUtf8(text_val.data(), text_val.size())) {}

SQLValue::SQLValue(const QByteArray& blob_val) : value_type(ValueType::BLOB), value(blob_val) {}
SQLValue::SQLValue(const std::vector<uint8_t>& blob_val)
    : value_type(ValueType::BLOB), value(conv_bytearray(blob_val)) {}

// SQLiteResultIterator
SQLiteResultIterator::SQLiteResultIterator(sqlite3_stmt* prepared_stmt, std::shared_ptr<int64_t> shared_idx,
                                           QVector<QString> cols, int rescode)
    : prepared_stmt(prepared_stmt), shared_idx(shared_idx), cols(cols), rescode(rescode) {
  current_idx = *shared_idx;

  fill_result();
}

SQLiteResultIterator::SQLiteResultIterator(int rescode) : rescode(rescode) {}

void SQLiteResultIterator::fill_result() const {
  if (rescode != SQLITE_ROW) return;

  result.resize(0);
  result.reserve(cols.size());
  for (int iCol = 0; iCol < cols.size(); iCol++) {
    switch ((SQLValue::ValueType)sqlite3_column_type(prepared_stmt, iCol)) {
      case SQLValue::ValueType::INT:
        result.emplace_back(SQLValue((int64_t)sqlite3_column_int64(prepared_stmt, iCol)));
        break;
      case SQLValue::ValueType::DOUBLE:
        result.emplace_back(SQLValue((double)sqlite3_column_double(prepared_stmt, iCol)));
        break;
      case SQLValue::ValueType::TEXT:
        result.emplace_back(SQLValue(std::string((const char*)sqlite3_column_text(prepared_stmt, iCol))));
        break;
      case SQLValue::ValueType::BLOB:
        result.emplace_back(SQLValue(QByteArray((const char*)sqlite3_column_blob(prepared_stmt, iCol),
                                                sqlite3_column_bytes(prepared_stmt, iCol))));
        break;
      case SQLValue::ValueType::NULL_VALUE:
        result.emplace_back(SQLValue());
        break;
    }
  }
}

SQLiteResultIterator& SQLiteResultIterator::operator++() {
  rescode = sqlite3_step(prepared_stmt);
  (*shared_idx)++;
  current_idx = *shared_idx;

  fill_result();
  return *this;
}

bool SQLiteResultIterator::operator==(const SQLiteResultIterator& lvalue) {
  return prepared_stmt == lvalue.prepared_stmt && current_idx == lvalue.current_idx;
}

bool SQLiteResultIterator::operator!=(const SQLiteResultIterator& lvalue) {
  if ((lvalue.rescode == SQLITE_DONE || lvalue.rescode == SQLITE_OK) &&
      (rescode == SQLITE_DONE || rescode == SQLITE_OK))
    return false;
  else if (prepared_stmt == lvalue.prepared_stmt && current_idx == lvalue.current_idx)
    return false;

  return true;
}

const SQLiteResultIterator::value_type& SQLiteResultIterator::operator*() const { return result; }

const SQLiteResultIterator::value_type* SQLiteResultIterator::operator->() const { return &result; }

SQLValue SQLiteResultIterator::operator[](size_t pos) const { return result[pos]; }

// SQLiteResult
SQLiteResult::SQLiteResult(sqlite3_stmt* prepared_stmt) : prepared_stmt(prepared_stmt) {
  rescode = sqlite3_step(prepared_stmt);
  shared_idx = std::make_shared<int64_t>();
  *shared_idx = 0;

  if (have_rows()) {
    int total_cols = sqlite3_column_count(prepared_stmt);
    cols.reserve(total_cols);
    for (int col_idx = 0; col_idx < total_cols; col_idx++)
      cols += sqlite3_column_name(prepared_stmt, col_idx);
  } else
    finalize();
}

SQLiteResult::~SQLiteResult() { finalize(); }

void SQLiteResult::finalize() {
  sqlite3_finalize(prepared_stmt);
  prepared_stmt = nullptr;
}

SQLiteResultIterator SQLiteResult::begin() { return SQLiteResultIterator(prepared_stmt, shared_idx, cols, rescode); }

SQLiteResultIterator SQLiteResult::end() { return SQLiteResultIterator(SQLITE_DONE); }

// SQLiteDB
SQLiteDB::SQLiteDB(const boost::filesystem::path& db_path) { sqlite3_open(db_path.string().c_str(), &db); }

SQLiteDB::~SQLiteDB() { sqlite3_close(db); }

SQLiteResult SQLiteDB::exec(const std::string& sql, const std::map<QString, SQLValue>& values) {
  sqlite3_stmt* sqlite_stmt;
  sqlite3_prepare_v2(db, sql.c_str(), (int)sql.size() + 1, &sqlite_stmt, nullptr);

  for (auto value : values) {
    std::string std_val = value.first.toStdString();
    switch (value.second.get_type()) {
      case SQLValue::ValueType::INT:
        sqlite3_bind_int64(sqlite_stmt, sqlite3_bind_parameter_index(sqlite_stmt, std_val.c_str()),
                           value.second.as_int());
        break;
      case SQLValue::ValueType::DOUBLE:
        sqlite3_bind_double(sqlite_stmt, sqlite3_bind_parameter_index(sqlite_stmt, std_val.c_str()),
                            value.second.as_double());
        break;
      case SQLValue::ValueType::TEXT: {
        auto text_data = value.second.as_text();
        sqlite3_bind_text64(sqlite_stmt, sqlite3_bind_parameter_index(sqlite_stmt, std_val.c_str()), text_data.data(),
                            text_data.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
      } break;
      case SQLValue::ValueType::BLOB: {
        auto blob_data = value.second.as_blob();
        sqlite3_bind_blob64(sqlite_stmt, sqlite3_bind_parameter_index(sqlite_stmt, std_val.c_str()), blob_data.data(),
                            blob_data.size(), SQLITE_TRANSIENT);
      } break;
      case SQLValue::ValueType::NULL_VALUE:
        sqlite3_bind_null(sqlite_stmt, sqlite3_bind_parameter_index(sqlite_stmt, std_val.c_str()));
        break;
    }
  }

  return SQLiteResult(sqlite_stmt);
}

SQLiteSavepoint::SQLiteSavepoint(SQLiteDB& db, QString savepoint_name) : db(db), name(std::move(savepoint_name)) {
  db.exec(std::string("SAVEPOINT ") + name.toStdString());
}
SQLiteSavepoint::~SQLiteSavepoint() { db.exec(std::string("ROLLBACK TO ") + name.toStdString()); }
void SQLiteSavepoint::commit() { db.exec(std::string("RELEASE ") + name.toStdString()); }

}  // namespace librevault
