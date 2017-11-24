#ifndef SELECTION_TOOL_H
#define SELECTION_TOOL_H

#include <vector>
#include <string>

#include <stdexcept>
#include <map>
// Structure measuring a range of a column
class Conditions {
public:
  // Way to add "and" to the output
  enum AndState {
    Neither,
    OnlyHead,
    OnlyTail,
    BothEnds,
  };
private:
  // true if values assigned, false if this object is not ready
  bool state_of_this_class;

  // Name of this column
  std::string title;

  // Low edge of this column. String rather than number due to the easiness of conversion
  std::string low_edge;

  // High edge of the column
  std::string high_edge;

  // Constructors
  // Default constructor
  Conditions();
  // Constructor with info
  Conditions(const std::string& name, const std::string& low, const std::string& high = "UNAVAILABLE");
  Conditions(const std::string& name, double low, double high = -1e9);

  static const std::string unlimited;
public:
  // Maintainer of state_of_this_class: ensure that all members are
  // successfully set by swapping reversed edges if necessary.
  // Returns true if all good, false otherwise
  bool ValidateCondition();
  // Const version, do the check only without modification even if not passed
  bool ValidCondition() const;

  // Convert to MySQL condition
  std::string Output(AndState concatenation = Neither) const;

  // Getters
  std::string Title() const { return title; }
  std::string LowEdge() const { return low_edge; }
  std::string HighEdge() const { return high_edge; }

  // Factory: condition generator
  // Exception class
  class BadGeneration: public std::logic_error {
  public:
    BadGeneration(const std::string& hint);
  };
  static Conditions Factory(const std::string& hint) throw(BadGeneration);
  enum CommentStatus {
    WithoutCommentsOnly,
    WithCommentsOnly,
    UnlimitedComments,
  };
  static void CommentConditions(int comment_status, std::string& command);

private:
  void Convert(double v, std::string& s);
  inline void AddQuote(const std::string& source, std::string& s);
public:
  class MapInitiater;
  friend class MapInitiater;
  class MapInitiater {
    MapInitiater();
    MapInitiater(const MapInitiater&);
    MapInitiater& operator=(const MapInitiater&);
  public:
    static void InitiateMap();
  };

  typedef void (Conditions::*string_processer)(std::string&);
private:
  void ProcessDate(std::string& date);
  void ProcessSize(std::string& size);
  static std::map<std::string, string_processer> function_pool;
  std::string GetTimeNow() const;
};


#include "common_tools.h"
class ArgumentParser: public BaseParser {
  // Pool for conditions from terminal
  std::vector<Conditions> conditions;

  // Output file settings
  // Don't print the result to the standard output if a file is used
  bool surpressing;
  // Name of an additional output file. Operational.
  std::string output_file;
  // Print level: only items with higher level will be printed.
  int print_level;
  // Comment status
  int comment_status;
  // Selecting rules for filename
  std::string file_hints;
public:
  // Default constructor
  ArgumentParser();

  // <Key method>
  // To parse terminal arguments and ingest the conditions into this class
  // Returns -10 in case of "--help", 0 if parsed and ready to continue. Other
  // values indicates an error
  int Parse(int argc, const char* argv[]);

  // Return the number of conditions
  size_t N_conditions() const { return conditions.size(); }

  // Getters:
  std::string Output() const { return output_file; }
  int PrintLevel() const { return print_level; }
  bool Surpressing() const { return surpressing; }
  Conditions GetOneCondition(size_t location) const
  { return location < N_conditions() ? conditions[location] : Conditions::Factory("UNDEFINED"); }

  // Return the MySQL query command generated using this->conditions
  // NULL string returned if this->conditions is empty or is flooded with
  // illegal conditions
  std::string MySQLCommand() const;

  // Give away the names of columns with printing level above "level"
  // Need two arguments:
  //   1. Target container of names
  //   2. Printing levels.
  //      -2:	Use print_level from ArgumentParser
  //      -1:	Take all columns
  //      0-3:	Use this level
  //      Others:	returns with an empty <target>
  // Nothing to return
  void Columns(std::vector<std::string>& target, int level = -2) const;	// name_of_columns
  std::string ColumnNames(int level = -2) const;
private:
  bool SetOutputFile(const std::string& filename, bool surpress);
  void AddFileHint(std::string& command) const;
};

class SelectionTool: MySQLInterface {
  // Members from MySQLInterface
  using MySQLInterface::mysqlInstance;
  using MySQLInterface::result;
  using MySQLInterface::errorNum;
  using MySQLInterface::errorInfo;
  using MySQLInterface::rows;
  using MySQLInterface::fields;

  // Buffer of records fetched from database
  RecordsHolder holder;

  // Info from ArgumentParser
  // Name of database
  std::string database;
  // MySQL query command
  std::string command;
  // Output file where records are to be saved
  std::string output;
  // Whether to surpress standard output in case of file savings
  bool surpressing;
public:
  // Constructor.
  SelectionTool();

  // Initiator of database. Ingest necessary settings from ArgumentParser
  // Needs one argument:
  //   1. Instance of ArgumentParser that parsed terminal arguments.
  // Return true if all done, false if something wrong
  bool InitiateDatabase(const ArgumentParser& parser);

  // Fetch record from databse, and print them to wherever wanted.
  // Nothing to return
  void SelectAndDisplay();
private:
  // Fetch record from databse.
  // Returns the number of records fetched.
  size_t FetchingRecords();

  // Print all record(s) fetched.
  // Called after FetchingRecords
  void Print(std::ostream& outflow) const
  { holder.Print(outflow); }
  void Print() const
  { holder.Print(); }
};

// Initializer used by Conditions::factory
#include <set>
class FactoryInitializer {
  std::set<std::string> allowed_titles;
  std::set<std::string> title_options;
  FactoryInitializer();
  FactoryInitializer(const FactoryInitializer&);
  FactoryInitializer& operator=(const FactoryInitializer&);
public:
  static FactoryInitializer& GetInstance();

  // Reserve a string as a title for Conditions
  void InsertTitle(const std::string& title);

  // Testing if <possible_title> has been reserved
  bool ValidTitle(const std::string& possible_title) const
  { return allowed_titles.end() != allowed_titles.find(possible_title); }

  // Iterator of allowed_titles, used by BaseParser
  std::set<std::string>::const_iterator option_begin() const
  { return title_options.begin(); }
  std::set<std::string>::const_iterator option_end() const
  { return title_options.end(); }
  std::set<std::string>::const_iterator title_begin() const
  { return allowed_titles.begin(); }
  std::set<std::string>::const_iterator title_end() const
  { return allowed_titles.end(); }
};
#endif // SELECTION_TOOL_H
