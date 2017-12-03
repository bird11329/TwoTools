#ifndef OPERATION_TOOL_H
#define OPERATION_TOOL_H

#include <vector>
#include <string>

// Extract values to insert from a json string
class RecordsHolder;
class ArgumentParser;
#include <map>
class PropertyParser {
  PropertyParser() {}
  PropertyParser(const PropertyParser&);
  PropertyParser& operator=(const PropertyParser&);
  std::map<std::string, int> columns;
public:
  static PropertyParser& GetInstance();

  void PrepareColumns(const ArgumentParser& parser);

  void ParseJSON(const std::string& value, RecordsHolder& holder) const;
  void ParseString(const std::map<std::string, std::string>& buffer, RecordsHolder& holder) const;
  inline int GetIndex(const std::string& one_column) const
  {
    std::map<std::string, int>::const_iterator it = columns.find(one_column);
    return columns.end() == it ? -1 : it->second;
  }
};


#include "common_tools.h"
class ArgumentParser: public BaseParser {
public:
  enum OperatingMode {
    undefined_mode,
    insertion_mode,
    updating_mode,
    deletion_mode,
  };
  enum TerminalInputMode {
    nothing_sent,
    json_file,
    direct_input,
    separate_columns,
  };
private:
  RecordsHolder holder;
  int operation_mode;
  bool separate_tables;
  int terminal_mode;
public:
  ArgumentParser();
  const RecordsHolder& Holder() const { return holder; }
  int Parse(int argc, const char* argv[]);
  int OperationMode() const { return operation_mode; }
  bool SeparateTables() const { return separate_tables; }
  int TerminalMode() const { return terminal_mode; }
private:
  bool IngestDirectInput();
  static const std::string separators[6];
  void InsertMap(const std::string& content, std::map<std::string, std::string>& target) const;
  bool OnDefaultAccounts() const
  {
    return Account::default_account != general_parameters[1];
  }
};

// Operations manager, relying on MySQLOperator to Insert/delete/update
class OperationTool {
public:
  // Exception class
  class BadCommand {};
private:
  // Base class for generating MySQL query command
  class BaseCommand {
  protected:
    // Constant strings whose names are self-explanatory.
    const std::string creation_time;
    const std::string data_2a_tbl;
    const std::string mc_simu_tbl;
    const std::string mc_reco_tbl;

    // Index of LFN in the vector received during real query
    int index_of_lfn;

    // Number of names of all columns with contents
    int number_of_columns_for_query;
    // The names of all columns with contents
    std::vector<std::string> columns_for_query;

    // Expected name of the table (from terminal arguments)
    std::string table;

    // Enable dynamically change of table if true
    bool different_tables;

  public:
    BaseCommand(const ArgumentParser& parser, const std::vector<int>& wanted);
    virtual ~BaseCommand();

    // Ensure reliability of <table>, implement leading_words(s)
    virtual bool InitiateDatabase() = 0;
    virtual std::string GetCommand(const std::vector<std::string>& source) = 0;

  protected:
    void ModifyCommand(std::string& command) const
    {
      CommonModify(command);
      SpecificModify(command);
    }
    // Change the format of <command>, for example, 'true' to 1.
    void CommonModify(std::string& command) const;
    virtual void SpecificModify(std::string& command) const = 0;

    bool ValidParameters(bool with_lfn = true) const;
    bool ValidTableName(const std::string& table_name) const;
    // Get time the moment it is called.
    std::string GetTime() const;
    std::string FindTable(const std::vector<std::string>& one_record) const;
    bool BasicCommand(const std::vector<std::string>& one_record, std::string& command) const;
  };
  class InsertionCommand: public BaseCommand {
    std::string leading_words;
  public:
    InsertionCommand(const ArgumentParser& parser, const std::vector<int>& wanted);
    bool InitiateDatabase();
    std::string GetCommand(const std::vector<std::string>& source) throw(BadCommand);
  private:
    void SpecificModify(std::string& command) const;
  };
  class UpdatingCommand: public BaseCommand {
    std::string leading_words_1;
    std::string leading_words_2;
  public:
    UpdatingCommand(const ArgumentParser& parser, const std::vector<int>& wanted);
    bool InitiateDatabase();
    std::string GetCommand(const std::vector<std::string>& source) throw(BadCommand);
  private:
    void SpecificModify(std::string& command) const;
  };
  class DeletionCommand: public BaseCommand {
    std::string leading_words;
  public:
    DeletionCommand(const ArgumentParser& parser, const std::vector<int>& wanted);
    bool InitiateDatabase();
    std::string GetCommand(const std::vector<std::string>& source) throw(BadCommand);
  private:
    void SpecificModify(std::string& command) const;
  };
  // Database manipulater
  class MySQLOperator: MySQLInterface {
    using MySQLInterface::mysqlInstance;
    using MySQLInterface::result;
    using MySQLInterface::errorNum;
    using MySQLInterface::errorInfo;
    using MySQLInterface::rows;
    using MySQLInterface::fields;
    bool accepted;
    int success;
    int failure;
    int the_mode;
    const ArgumentParser& parser;
    bool (MySQLOperator::*real_method)(const std::string&);
  public:
    MySQLOperator(const ArgumentParser& p);
    ~MySQLOperator();
    // Initiator of database. Ingest necessary settings from ArgumentParser
    // Needs one argument:
    //   1. Instance of ArgumentParser that parsed terminal arguments.
    // Return true if all done, false if something wrong
    bool InitiateDatabase();

    // Generate the MySQL query command generator
    BaseCommand* Generate(const std::vector<int>& indexes) const;

    // Insert one record into database each time
    void OneRecord(const std::string& the_command);

    // To confirm that all the records are to be inserted into database
    void Accepted(bool v = true) { accepted = v; }

    // Add the counts for failures
    void FailedRecord() { ++failure; }
  };
  const RecordsHolder& holder;
public:
  OperationTool(const RecordsHolder& h);

  // Additional permission check for OperationTool
  bool Authentication() const;
  // Returns true if all done, false otherwise.
  bool Run(const ArgumentParser& parser);
};

#endif // OPERATION_TOOL_H
