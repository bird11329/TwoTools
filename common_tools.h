#ifndef COMMON_TOOLS_H
#define COMMON_TOOLS_H

#include <vector>
#include <string>

// Configuration information
class Configuration {
  Configuration();
  Configuration(const Configuration&);
  Configuration& operator=(const Configuration&);
  std::string expected_key;
public:
  static Configuration& GetInstance();
  std::string ConfigurationFolder() const;
  std::string InstallationFolder() const;
  const char* Authentication() const;
  bool Authenticating(const std::string& real_key) const;
};

namespace Account {
  // Default account for database
  const std::string default_account = "anonymous";
  // Password for default account
  const std::string default_password = "testing";
}




#include <mysql/mysql.h>
// MySQL interface. Designed to be inherited.
class MySQLInterface {
protected:
  // MySQL object. Necessary
  MYSQL mysqlInstance;

  // Record(s) fetched (similar to an array of char*)
  MYSQL_RES *result;

  // Error code
  int errorNum;

  // Error message
  const char* errorInfo;

  // Number of records
  unsigned long rows;

  // Number of columns
  unsigned long fields;

  // Constructor
  MySQLInterface();
  // Destructor
  virtual ~MySQLInterface();

  // Connecting database
  bool connectMySQL(const char* server,
                    const char* username = Account::default_account.c_str(),
                    const char* passwd = Account::default_password.c_str(),
                    const char* database = "DAMPE_GENEVA",
                    int port = 3306);

  // Change database
  int SelectDB(const char* database)
  { return ::mysql_select_db(&mysqlInstance, database); }

  // Change user
  int ChangeUser(const char* user, const char* passwd, const char* database = 0)
  { return ::mysql_change_user(&mysqlInstance, user, passwd, database); }

  // Retrieve data from database
  bool GetDataFromDB(const std::string&,
                     std::vector<std::vector<std::string> >&);

  // Either of the three methods takes mysql query command as the argument, and
  // their implementation looks so similar that mix-use only brings about
  // contextual concerns.
  // Returns false if query is rejected or if query fails, true if executed
  // Inserter
  bool Insert(const std::string& queryStr);
  // Updater
  bool Update(const std::string& queryStr);
  // Delete
  bool Delete(const std::string& queryStr);

  // MySQL query command executer. Handles any command.
  bool DirectQuery(const std::string& queryStr);

  // Clear the buffer within
  void ClearData()
  {
    if(result) ::mysql_free_result(result); // Release the sources
    result = 0;
  }

  // Getters
  int GetErrorNum() const { return errorNum; }
  const char* GetErrorInfo() const { return errorInfo; }
  unsigned long GetNRows() const { return rows; }
  unsigned long GetNFields() const { return fields; }
  unsigned long GetAffectRows() const { return rows; }

  // Returns the number of records affected by the latest query command (for
  // insert/delete/update)
  unsigned long GetInsertedID()
  { return ::mysql_insert_id(&mysqlInstance); }

  // Print name of columns on which last "select" concerns
  void PrintNameOfColumns() const;

  // Retrieve name of columns on which last "select" concerns
  // Returns the status of streaming the records
  bool GetNameOfColumns(std::vector<std::string>&) const;

  // Print the error message of last MySQL query if any
  void errorIntoMySQL();

public:
  // Print content retrieved from mysql query. "NULL" for no data.
  // 2 arguments:
  //   1: vector of vector of string of contents from mysql query. Inner vectors
  //      shall have identical number of elements.
  //   2: name of columns in the query. Can be empty, and thus not displayed.
  static void PrintingValues( const std::vector<std::vector<std::string> >& data, const std::vector<std::string>& names_of_column);

private:
  void closeMySQL();
  bool GenerateNameOfColumns(std::vector<std::string>&) const;
  bool TestingQueryCommand(const std::string&, const std::string&);
  bool Non_Select_Query(const std::string&);
};

namespace boost {
  namespace program_options {
    class options_description;
    class variables_map;
  }
}


// Records holder. Save records, either retrieved from database and then to be
// printed, or to be inserted into database.
#include <iosfwd>
class BaseParser;
class RecordsHolder {
  // All records
  std::vector<std::vector<std::string> > records;
  // First two rows of records, trying to remove repetition
  std::vector<std::string> first_column;
  // Number of vector of string in records. Same to the size of first_column
  size_t number_of_records;
  // Names of columns
  std::vector<std::string> name_of_columns;
  // Number of columns. Remain still after initialization.
  size_t number_of_columns;
public:
  // (Default) constructor
  RecordsHolder();

  // Initialize name and number of columns, make ready the two vector
  bool Initialize(const BaseParser& parser);

  // Reserve memory for records and first_column;
  // Only the size of records and first_column
  void Reserve(size_t first)
  {
    if(1e6 < first) return;
    records.reserve(first);
    first_column.reserve(first);
  }

  // Getters of two scales
  size_t N_records() const { return number_of_records; }
  size_t N_columns() const { return number_of_columns; }

  // Insert a record into RecordsHolder
  // Insert with a vector (v1)
  void Insert(const std::vector<std::string>& record, int index_of_lfn = -2);
  // Insert with a string separated by <separator> (v2)
  void Insert(const std::string& record, const std::string& separator = ";");

  // Retrieve a record from RecordsHolder, index as the argument
  // Retrieve using a vector.
  // If wrong location, empty <target> results.
  void GetRecord(size_t location, std::vector<std::string>& target) const;

  // Retrieve by a string (a vector joined)
  // If wrong <location>, a null string returns.
  std::string GetRecord(size_t location) const;

  // Output the records within
  void Print() const;
  void Print(std::ostream& outflow) const;
private:
  bool ValidLocation(size_t location) const
  { return location < N_records(); }
  int FindLFN() const;
};



#include <stdexcept>
// Argument parser base. Contains general parameters shared by both selection
// tool and operation tool
class BaseParser {
  // Use a query to find the structure of the table
  class ColumnAcquirer: MySQLInterface {
    using MySQLInterface::mysqlInstance;
    using MySQLInterface::result;
    using MySQLInterface::errorNum;
    using MySQLInterface::errorInfo;
    using MySQLInterface::rows;
    using MySQLInterface::fields;
    std::vector<std::string> names;
    std::vector<int> levels;
  public:
    ColumnAcquirer();
    bool Acquire(const std::string& server,
                 const std::string& database,
                 int port, const std::string& table);
    bool Deliver(std::vector<std::string>& o_names, std::vector<int>& o_levels) const;
  };

protected:
  // Only general parameters used by both SelectionTool and OperationTool
  // 1-5: <string>, use an array for convenience
  std::string general_parameters[5];
  // [0] Server, like xxx.xxx.xxx.xxx
  // [1] User account
  // [2] Password for this user to log in
  // [3] Name of database
  // [4] Name of table

  // 6. Port
  int port;
  // 7. Names of columns
  std::vector<std::string> name_of_columns;
  // 8. Number of columns. Remain still after initialization.
  size_t number_of_columns;
  // 9. Printing level of columns
  std::vector<int> level_of_columns;

  const std::string trivial_account;
  const std::string trivial_password;
  // Constructor
  BaseParser();
  // Destructor
  virtual ~BaseParser();

  // <Key method>
  // To parse terminal arguments and ingest the conditions into this class
  // Returns -10 in case of "--help", 0 if parsed and ready to continue. Other
  // values indicates an error
  virtual int Parse(int argc, const char* argv[]) = 0;

  // Prepare code for general parameters parser (1-7) (8, 9 omitted, because
  // they are not used by end users)
  // Called before the real query and the parsing of terminals
  void GeneralAssembler(boost::program_options::options_description& opts) const throw(std::runtime_error);

  // Components for general parameters parser (1-7)
  // Called before the real query and after the parsing of terminals
  void GetGeneralParameters(const boost::program_options::variables_map& v_map, int level = -1) throw(std::runtime_error);
  // On default account. Two tools take it differently: SelectionTool accepts
  // it, while OperationTool rejects it. 
  virtual bool OnDefaultAccounts() const { return true; }

  // Reserve repulsive options for future check
  void ReserveRepulsiveOptions(const char* option1, const char* option2) const
  {
    // Efficiency concerns, no repetition check.
    repulsive_options.push_back(option1);
    repulsive_options.push_back(option2);
  }
private:
  // Take in password from terminal interactively
  void InteractivePassword();

  // (For server:) turn domain name into string of 4 numbers (IP)
  void AcquiringRealServer();

  // Title of general parameter names
  static const std::string general_parameter_names[6];
  // Read <option_file> for login info such as user, passwd, database, etc.
  void IngestOptions(std::string option_file);
  enum OptionFileState {
    SuccessfulAccess,
    NoSuchFile,
    IllegalFile,
  };
  int GuessOptionFileName(std::string& option_file) const;

  // Pair(s) of options that are repulsive to each other
  mutable std::vector<std::string> repulsive_options;
  // Returns if there are repulsive options
  // Called before the real query and after the parsing of terminals by GetGeneralParameters
  std::string ReplusiveOptions(const boost::program_options::variables_map& v_map);

  // Components for general parameters parser (7 - 9)
  // Called before the real query and after the parsing of terminals by GetGeneralParameters
  void PrepareColumns() throw(std::runtime_error);

public:
  // Returns true if not parsed.
  bool NotReady() const
  { return general_parameters[3].empty(); }

  // Estimate the scale of this query (used by RecordsHolder below).
  size_t N_Records() const;

  // Getters
  std::string Server() const { return general_parameters[0]; }
  std::string User() const { return general_parameters[1]; }
  std::string Passwd() const { return general_parameters[2]; }
  std::string Database() const { return general_parameters[3]; }
  std::string Table() const { return general_parameters[4]; }
  int Port() const { return port; }
  virtual void Columns(std::vector<std::string>& target, int level = -2) const;	// name_of_columns
  size_t N_Columns() const { return number_of_columns; }
};

#endif // COMMON_TOOLS_H
