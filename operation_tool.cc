// ************************************************************************* //
// File: operation_tool.cc
// Contents: modules used by OperationTool
// Classes:
//   PropertyParser
//   ArgumentParser
//   OperationTool::BaseCommand
//   OperationTool::InsertionCommand
//   OperationTool::UpdatingCommand
//   OperationTool::DeletionCommand
//   TrivialErrorNullification
//   OperationTool::MySQLOperator
//   OperationTool
// ************************************************************************* //

#include "operation_tool.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <sstream>
#include <ctime>
using namespace std;


// ===> PropertyParser <===
// Parse a JSON string, or a string, and save the result into RecordsHolder
// Singletonship
PropertyParser& PropertyParser::GetInstance()
{
  static PropertyParser instance;
  return instance;
}

// <Key method> (v1)
// Parse JSON line <value> and save the results into <holder>
// Need 2 arguments:
//   1. JSON string
//   2. RecordsHolder where the result is stored
// On format of JSON:
//   1. list ([JSON1, JSON2, ...]) of JSON is supported
//   2. true/false expected, but True/False is supported
void PropertyParser::ParseJSON(const string& value, RecordsHolder& holder) const
{
  if(value.empty())
    return;

  vector<string> all_strings;
  // Support for python lists
  // A python list must start with a '[' and must ends with a ']', or it's not
  // recognized.
  if(string::npos != value.find("}, {") && '[' == value[0] && boost::ends_with(value, "]"))
  {
    string splitted(boost::replace_all_copy(value, "}, {", "}#{"));
    // To support many lists in a line.
    boost::replace_all(splitted, "},{", "}#{");
    boost::replace_all(splitted, "], [", "]#[");
    boost::replace_all(splitted, "] , [", "]#[");
    boost::replace_all(splitted, "] ,[", "]#[");
    boost::replace_all(splitted, "],[", "]#[");
    boost::erase_last(splitted, "]");
    splitted.erase(0, 1);
    boost::split(all_strings, splitted, boost::is_any_of("#"));
  }
  else
    all_strings.push_back(value);

  for(vector<string>::iterator it = all_strings.begin(); it != all_strings.end(); ++it)
  {
    string c_formated(*it);
    // Basic replacement of some unrecognized items from python, etc.
    if(string::npos != c_formated.find("True"))
      boost::replace_all(c_formated, "True", "true");
    if(string::npos != c_formated.find("False"))
      boost::replace_all(c_formated, "False", "false");
    if(string::npos != c_formated.find("'"))
      boost::replace_all(c_formated, "'", "\"");

    // After the replacement, parse the JSON string
    stringstream ss(c_formated);
    boost::property_tree::ptree pt;
    try {
      boost::property_tree::read_json(ss, pt);
    } catch(boost::property_tree::ptree_error& pt_e) {
      cout << "\e[31mError parsing \"\e[0m\e[32m" << value << "\e[0m\e[31m\"...\e[0m" << endl;
      cout << "\e[33m" << pt_e.what() << "\e[0m" << endl;
      return;
    }

    // Store the results with a map
    map<string, string> one_record;
    for(boost::property_tree::ptree::iterator it = pt.begin(); it != pt.end(); ++it)
      one_record.insert(make_pair(it->first, it->second.data()));

    ParseString(one_record, holder);
  }
}
// <Key method> (v2)
// Parse directly a string and save the results into <holder>
// Need 2 arguments:
//   1. key-value pairs
//   2. RecordsHolder where the result is stored
// On format of buffer
//   1. key shall be one of the columns
//   2. there must be an "lfn" key
void PropertyParser::ParseString(const map<string, string>& buffer, RecordsHolder& holder) const
{
  if(buffer.empty())
    return;

  vector<string> one_record(columns.size());
  int inserted = -1;
  for(map<string, string>::const_iterator it = buffer.begin(); it != buffer.end(); ++it)
  {
    map<string, int>::const_iterator position = columns.find(it->first);
    if(columns.end() == position)
      continue;
    if(!it->second.empty())
    {
      one_record[position->second].assign(it->second);
      if("lfn" == it->first)
      {
        // lfn found.
        inserted = position->second;
        one_record.back().assign(
            boost::istarts_with(it->second, "root://") ? "1" : "0"
        );
      }
    }
  }

  if(-1 != inserted)
    holder.Insert(one_record, inserted);
}

// Take in names of all columns
void PropertyParser::PrepareColumns(const ArgumentParser& parser)
{
  vector<string> all_columns;
  parser.Columns(all_columns);
  int n = all_columns.size();
  for(int i=0;i<n;++i)
    columns.insert(make_pair(all_columns[i], i));
}


// ===> ArgumentParser <===
// Parse the terminal arguments if any
// Constructor
ArgumentParser::ArgumentParser()
{
  operation_mode = undefined_mode;
  terminal_mode = nothing_sent;
}

// Key method
// Need 2 arguments:
//   1. argc (of main)
//   2. argv (of main)
// Returns 0 if success, non-0 otherwise
int ArgumentParser::Parse(int argc, const char* argv[])
{
  if(1 == argc)
  {
    cout << "Need at least an argument to run this program..." << endl;
    return 1;
  }

  // Create description of all allowed options
  boost::program_options::options_description opts("OperationTool: Database handler (Superuser only)");
  try {
    GeneralAssembler(opts);
  } catch(runtime_error& e) {
    cerr << "Error preparing parser..." << endl;
    cerr << e.what() << endl;
    return 1;
  }

  // For later use, adding new items (options)
  boost::program_options::options_description_easy_init adding = opts.add_options();

  using boost::program_options::value;

  // Adding items
  adding.operator()
    ("json-file,j", value<string>(), "Name of JSON file to be parsed")
    ("direct-input,d", "Enable user(s) to input the JSON string from terminal")
    ("mode,M", value<int>()->default_value(1), "Insert(default)(1)/Update(2)/Delete(3)")
    ("delete-mode", "Switch to delete mode")
    ("update-mode", "Switch to update mode")
    ("multi-tables", "Allowing different types of ROOT files")
    ("lfn,f", value<string>(), "Logical file name")
    ("tstop,e", value<string>(), "Timestamp of last event")
    ("tstart,b", value<string>(), "Timestamp of first event")
    ("size,s", value<string>(), "Size of this file (unit: B)")
    ("nevts,n", value<string>(), "Number of events")
    ("SvnRev,v", value<string>(), "Revision version of DAMPE (i.e., 6047)")
    ("version,V", value<string>(), "Version DAMPE (i.e., 5.4.2)")
    ("comment,c", value<string>(), "Comment of this file")
    ("emax,H", value<string>(), "Highest energy in the file")
    ("emin,L", value<string>(), "Lowest energy in the file")
    ("last_modified,l", value<string>(), "Time of last modified (\"YYYYmmdd-HHMMSS\")")
    ("checksum,C", value<string>(), "Expected CheckSum of ROOT file (2A only)")
  ;

  // Reserve repulsive options
  ReserveRepulsiveOptions("delete-mode", "update-mode");
  ReserveRepulsiveOptions("json-file", "direct-input");
  string items[] = { "lfn", "tstop", "tstart", "size", "nevts", "SvnRev", "version", "comment", "emax", "emin", "last_modified", "checksum" };
  int N_items = sizeof(items)/sizeof(items[0]);
  for(int i=0;i<N_items;++i)
    ReserveRepulsiveOptions("json-file", items[i].c_str());

  // Parse the arguments
  boost::program_options::variables_map vm;
  try {
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
  } catch(exception& e) {
    cerr << "Error parsing arguments..." << endl;
    cerr << e.what() << endl;
    return 1;
  }

  // If "help""manual", display it and return -10.
  if(vm.count("help"))
  {
    // If there is a "help", help, and exit (-10 affects how the main function behaves)
    cout << opts << endl;
    cout << "(For detailed help, try using \"-m\".)" << endl;
    return -10;
  }
  else if(vm.count("manual"))
  {
    // Print detailed help messages
    ifstream in("OperationTool.help");
    if(!in.is_open())
      in.open("../OperationTool.help");
    if(in.is_open())
    {
      cout << in.rdbuf() << endl;
      in.close();
      return -10;
    }
    else
    {
      cout << "Error! Can't find help messages for OperationTool..." << endl;
      return 2;
    }
  }

  // Get general parameters used by both OperationTool (also used by
  // SelectionTool)
  try {
    GetGeneralParameters(vm);
  } catch(runtime_error& e) {
    cerr << "Parsing terminal arguments failed..." << endl;
    cerr << e.what() << endl;
    return 2;
  }

  // Take in names of columns, etc.
  PropertyParser::GetInstance().PrepareColumns(*this);

  // Find operation mode: insert/update/delete
  try {
    operation_mode = vm["mode"].as<int>();
  } catch(exception& e) {
    cerr << "Illegal arguments: " << vm["comment-state"].as<string>() << endl;
    cerr << e.what() << endl;
    return 2;
  }

  // Try to acquire the mode
  string two_modes[] = { "insert-mode", "update-mode", "delete-mode" };
  for(int i=1;i<3;++i)
  {
    if(0 == vm.count(two_modes[i].c_str()))
      continue;

    if(insertion_mode != operation_mode && i+1 != operation_mode)
    {
      cerr << "Collision detected!!! Both \"" << two_modes[i] << "\" and mode = " << operation_mode << " received." << endl;
      return 2;
    }

    operation_mode = i+1;
  }

  // Multi-tables mode: items for different tables
  // If not separate_tables, if a record's table differs from that from
  // terminal, OperationTool quits.
  separate_tables = vm.count("multi-tables");

  // Ways to give OperationTool the materials to handle
  if(vm.count("json-file"))
  {
    // Use of json-file
    string json_file(vm["json-file"].as<string>());
    ifstream in(json_file.c_str());
    if(!in || !in.is_open())
    {
      cerr << "Error openning json file |" << json_file << "|..." << endl;
      return 2;
    }
    string line;
    while(getline(in, line))
    {
      if(3 > line.size()) break;
      PropertyParser::GetInstance().ParseJSON(line, holder);
      line.clear();
    }
    in.close();
    terminal_mode = ArgumentParser::json_file;
  }
  else if(vm.count("direct-input"))
  {
    // Materials will be input interactively (or use pipeline or something)
    if(!IngestDirectInput())
    {
      cerr << "Failed in parsing direct terminal input..." << endl;
      return 2;
    }
    terminal_mode = direct_input;
  }
  else
  {
    // Use an option to specify a key-value pair, and all options are taken as
    // one record
    map<string, string> insertion;
    for(int i=0;i<N_items;++i)
    {
      if(vm.count(items[i]))
        insertion.insert(make_pair(items[i], vm[items[i]].as<string>()));
//    {
//      insertion.insert(make_pair(items[i], vm[items[i]].as<string>()));
//      cout << items[i] << ": |" << vm[items[i]].as<string>() << "| inserted." << endl;
//    }
    }
    if(insertion.empty())
    {
      // Nothing from options
      cerr << "Nothing to insert..." << endl;
      cerr << "Either a json-file, or manual input is necessary." << endl;
      return 2;
    }
    else if(!vm.count("lfn"))
    {
      // Receives something but no lfn is available
      cerr << "Logical File Name is necessary while inserting by terminal..." << endl;
      return 2;
    }
    else
    {
      // Parsing the record from options
      if(insertion.end() != insertion.find("comment"))
      {
        string comment = insertion["comment"];
        if(boost::iequals(comment, "NULL") || boost::iequals(comment, "none"))
          insertion.insert(make_pair("error_code", "1"));
      }
      PropertyParser::GetInstance().ParseString(insertion, holder);
      terminal_mode = separate_columns;
    }
  }

  return 0;
}

// Direct input...
bool ArgumentParser::IngestDirectInput()
{
  string line;
  map<string, string> one_record;

  // Use a blank line to terminate. Signal Ctrl + D also works (can break this
  // loop).
  while(getline(cin, line))
  {
    if(line.size() < 2)
      break;

    // Each pair of (<aspect>, <value>) is transferred to "InsertMap" to get
    // parsed.
    if(string::npos != line.find(";") || string::npos != line.find(","))
    {
      // A line with multiple aspects
      vector<string> contents;
      boost::split(contents, line, boost::is_any_of(";,"), boost::token_compress_on);
      for(vector<string>::iterator it = contents.begin(); it != contents.end(); ++it)
        InsertMap(*it, one_record);
    }
    else
      InsertMap(line, one_record);

    line.clear();
  }

  if(one_record.end() == one_record.find("lfn"))
  {
    cerr << "No LFN found in the JSON string |" << line << "|..." << endl;
    return false;
  }
  else
  {
    PropertyParser::GetInstance().ParseString(one_record, holder);
    return true;
  }
}

// Insert into the key-value container. This container as a whole is a record.
void ArgumentParser::InsertMap(const string& content, map<string, string>& target) const
{
  if(content.empty())
    return;

  string line(content);
  // Replace single quotes with double ones.
  if(string::npos != line.find("'"))
    boost::replace_all(line, "'", "\"");

  // Find the separator used by this pair to separate aspect and value.
  int found = -1;
  for(int i=0;i<sizeof(separators)/sizeof(separators[0]);++i)
  {
    if(string::npos != line.find(separators[i]))
    {
      if(-1 != found && 4 != i && 0 == found)
      {
        cout << "Multiple separators found (Before: " <<
                separators[found] << "; Now: " << separators[i] << endl;
      }
      found = i;
    }
  }
  if(-1 == found)
  {
    // No separator matches. Can't extract aspects nor values
    cout << "Warning... No column specified from |" << line << "|..." << endl;
    return;
  }

  // Separate aspect and value
  string second = line.substr(line.find(separators[found]) + separators[found].size());
  string first = line.substr(0, line.find(separators[found]));
  if(second.empty())
  {
    cout << "Empty data on " << first << endl;
    return;
  }

  if(found)
  {
    boost::erase_all(first, "\"");
    boost::erase_all(second, "\"");
  }
  // To remove leading and tailing whitespaces
  boost::trim(first);
  boost::trim(second);

  if(target.end() != target.find(first))
  {
    cout << "Overwritting item of " << first << " (" << target[first] << ") with " << second;
    target[first].assign(second);
  }
  else
    target.insert(make_pair(first, second));
}

// Allowed separators of JSON string
const string ArgumentParser::separators[] = {"=", "\": \"", "\":\"", "\" :\"", "\" : \"", "\": "};


// ===> OperationTool::BaseCommand <===
// Generates the forewords (column names) for MySQL queries
// Locate the index of LFN, hints to find the table
// Need 2 arguments:
//   1. ArgumentParser
//   2. indexes of wanted columns among all columns (to avoid NULL)
OperationTool::BaseCommand::BaseCommand(const ArgumentParser& parser, const vector<int>& wanted):
  creation_time(GetTime()),
  data_2a_tbl("data_2a_tbl"),
  mc_simu_tbl("mc_simu_tbl"),
  mc_reco_tbl("mc_reco_tbl"),
  index_of_lfn(-1),
  different_tables(false),
  number_of_columns_for_query(parser.N_Columns()),
  columns_for_query(number_of_columns_for_query)
{
  vector<string> column_names;	// List of names of columns
  parser.Columns(column_names);
  size_t N_columns = wanted.size();
  if(parser.N_Columns() < N_columns)
  {
    cerr << "FATAL ERROR!!! " << N_columns << " <==> " << parser.N_Columns() << endl;
    exit(1);
  }
  if(parser.N_Columns() == N_columns)
  {
    // In case that all columns are wanted
    for(size_t i=0;i<N_columns;++i)
    {
      const string& one_column = column_names[i];
      if("lfn" == one_column)
      {
        if(-1 != index_of_lfn)
          cout << "Multiple LFN detected: " << index_of_lfn << ", and " << i << endl;
        index_of_lfn = i;
      }
      columns_for_query[i].assign(one_column);
    }
  }
  else
  {
    // In case that some columns are believed to be NULL
    for(size_t i=0;i<N_columns;++i)
    {
      const string& one_column = column_names[wanted[i]];
      if("lfn" == one_column)
      {
        if(-1 != index_of_lfn)
          cout << "Multiple LFN detected: " << index_of_lfn << ", and " << i << endl;
        index_of_lfn = wanted[i];
      }
      columns_for_query[i].assign(one_column);
    }
  }

  different_tables = parser.SeparateTables();
  table.assign(parser.Table());
}

// (Trivial) Destructor
OperationTool::BaseCommand::~BaseCommand()
{}

// Test if table name is good, and if there is a valid index_of_lfn
bool OperationTool::BaseCommand::ValidParameters(bool with_lfn) const
{
  // Testing "table"
  if(table.empty() || !ValidTableName(table))
  {
    cerr << "Error getting table (" << table << ")..." << endl;
    return false;
  }

  // Verifying "index_of_lfn" if necessary
  if(-1 == index_of_lfn && with_lfn)
  {
    cerr << "No LFN found from all columns..." << endl;
    return false;
  }

  return true;
}

// Test if name of table from terminal is one of the three
bool OperationTool::BaseCommand::ValidTableName(const string& table_name) const
{
  string all_names[] = {data_2a_tbl, mc_simu_tbl, mc_reco_tbl};
  for(int i=0;i<3;++i)
  {
    if(table_name == all_names[i])
      return true;
    all_names[i].insert(0, "test_");
    if(table_name == all_names[i])
    {
      cerr << "Testing database |" << all_names << "| used." << endl;
      return true;
    }
  }

  cerr << "\e[31mInvalid table name |" << table_name << "|...\e[0m" << endl;
  return false;
}

// Get current time (YYYYmmdd_HHMMSS)
string OperationTool::BaseCommand::GetTime() const
{
  time_t rawtime;
  time(&rawtime);
  struct tm * timeinfo = localtime(&rawtime);
  char str[20] = {'\0'};
  strftime(str, 16, "%Y%m%d_%H%M%S", timeinfo);
  return str;
}

// Parse the LFN to get the name of table
string OperationTool::BaseCommand::FindTable(const vector<string>& one_record) const
{
  if(-1 == index_of_lfn || one_record.size() <= index_of_lfn)
    return string();

  if(string::npos != one_record[index_of_lfn].find("DAMPE_2A_OBS"))
    return data_2a_tbl;
  else if(string::npos != one_record[index_of_lfn].find(".mc.root"))
    return mc_simu_tbl;
  else if(string::npos != one_record[index_of_lfn].find(".reco.root"))
    return mc_reco_tbl;
  else
    return string();
}

// Testing if the table of the record presented by <one_record> matches
// Need 2 arguments:
//   1. a record (an aspect as an element)
//   2. MySQL query command for this record
// Returns true if accepted (table from terminal and table of this record
// match, or different tables are allowed), false otherwise
bool OperationTool::BaseCommand::BasicCommand(const vector<string>& one_record, string& command) const
{
  if(one_record.empty())
  {
    cout << "Nothing from the source..." << endl;
    return false;
  }

  string name_of_table(FindTable(one_record));
  if(name_of_table.empty())
  {
    cout << "No table found." << endl;
    return false;
  }

  if(name_of_table != table)
  {
    if(!different_tables)
    {
      cerr << "Warning... Terminal table |" << table << "| and parsed one |" <<
              name_of_table << "| don't match!" << endl;
      cout << "Return with nothing done." << endl;
      return false;
    }
    else
    {
      cerr << "Warning... Terminal table |" << table << "| and parsed one |" <<
              name_of_table << "| don't match... Updating..." << endl;
      boost::replace_all(command, table, name_of_table);
    }
  }

  return true;
}

// Modifying the command
void OperationTool::BaseCommand::CommonModify(string& command) const
{
  if(string::npos != command.find("'true'"))
    boost::replace_all(command, "'true'", "'1'");
  if(string::npos != command.find("'false'"))
    boost::replace_all(command, "'false'", "'0'");
}


// ===> OperationTool::MySQLOperator::InsertionCommand <===
// Insertion command generator
OperationTool::InsertionCommand::InsertionCommand(const ArgumentParser& parser, const vector<int>& wanted):
  BaseCommand(parser, wanted),
  leading_words("insert into ")	// Common words for querying each record
{}

// Initiate the database, and prepare the leading_words
bool OperationTool::InsertionCommand::InitiateDatabase()
{
  if(!ValidParameters())
    return false;

  leading_words.append(table);
  leading_words.append(" (");
  leading_words.append(boost::join(columns_for_query, ", "));
  leading_words.append(") values ('");
  return true;
}

// Use source, a record, and leading_words, etc., to generate a MySQL query command
string OperationTool::InsertionCommand::GetCommand(const vector<string>& source) throw(OperationTool::BadCommand)
{
  string one_command(leading_words);
  if(!BasicCommand(source, one_command))
    throw BadCommand();

  one_command.append(boost::join(source, "', '"));
  one_command.append("')");

  ModifyCommand(one_command);

  return one_command;
}

// Fix some error induced while modifying MySQL query command
void OperationTool::InsertionCommand::SpecificModify(string& command) const
{
  bool replaced = false;
  while(string::npos != command.find(", ,"))
  {
    boost::replace_all(command, ", ,", ",");
    replaced = true;
  }
  if(replaced)
  {
    boost::replace_all(command, ", ) values", ") values");
    boost::erase_all(command, ", ''");
    boost::erase_all(command, "'', ");
  }
}


// ===> OperationTool::UpdatingCommand <===
// Update command generator
OperationTool::UpdatingCommand::UpdatingCommand(const ArgumentParser& parser, const vector<int>& wanted):
  BaseCommand(parser, wanted),
  leading_words_1("update "),
  leading_words_2(" where lfn = '")
{ }

// Initiate the database, and prepare the leading_words
bool OperationTool::UpdatingCommand::InitiateDatabase()
{
  if(!ValidParameters())
    return false;

  leading_words_1.append(table);
  leading_words_1.append(" set ");
  return true;
}

// Use source, a record, and leading_words, etc., to generate a MySQL query command
string OperationTool::UpdatingCommand::GetCommand(const vector<string>& source) throw(BadCommand)
{
  if(source.size() != number_of_columns_for_query)
  {
    cout << "Warning! Number of columns doesn't match! " << source.size() << " from record while " << columns_for_query.size() << " internally." << endl;
    throw BadCommand();
  }

  string one_command(leading_words_1);
  if(!BasicCommand(source, one_command))
    throw BadCommand();

  for(int i=0;i<number_of_columns_for_query;++i)
  {
    if(columns_for_query[i].empty())
      continue;

    int index_of_this_column = PropertyParser::GetInstance().GetIndex(columns_for_query[i]);
    if(-1 == index_of_this_column || index_of_this_column == index_of_lfn)
      continue;
    one_command.append(columns_for_query[i]);
    if(source[index_of_this_column].empty())
      one_command.append(" is NULL, ");
    else
    {
      one_command.append(" = '");
      one_command.append(source[index_of_this_column]);
      one_command.append("', ");
    }
  }
  boost::erase_last(one_command, ", ");

  one_command.append(leading_words_2);
  one_command.append(source[index_of_lfn]);
  one_command.append("'");

  ModifyCommand(one_command);

//cout << "Updating command: |" << one_command << "|." << endl;
  return one_command;
}

void OperationTool::UpdatingCommand::SpecificModify(string& command) const
{ }


// ===> OperationTool::DeletionCommand <===
// Delete command generator
OperationTool::DeletionCommand::DeletionCommand(const ArgumentParser& parser, const vector<int>& wanted):
  BaseCommand(parser, wanted),
  leading_words("delete from ")
{ }

// Initiate the database, and prepare the leading_words
bool OperationTool::DeletionCommand::InitiateDatabase()
{
  if(!ValidParameters(false))
    return false;

  leading_words.append(table);
  leading_words.append(" where ");
  return true;
}

// Use source, a record, and leading_words, etc., to generate a MySQL query command
string OperationTool::DeletionCommand::GetCommand(const vector<string>& source) throw(BadCommand)
{
  string one_command(leading_words);
  if(!BasicCommand(source, one_command))
    throw BadCommand();

  for(int i=0;i<number_of_columns_for_query;++i)
  {
    if(columns_for_query[i].empty())
      continue;

    int index_of_this_column = PropertyParser::GetInstance().GetIndex(columns_for_query[i]);
    if(-1 == index_of_this_column)
      continue;
    one_command.append(columns_for_query[i]);
    if(source[index_of_this_column].empty())
      one_command.append(" is NULL and ");
    else
    {
      one_command.append(" = '");
      one_command.append(source[index_of_this_column]);
      one_command.append("' and ");
    }
  }
  boost::erase_last(one_command, " and ");

  ModifyCommand(one_command);

//cout << "Deleting command: |" << one_command << "|." << endl;
  return one_command;
}

void OperationTool::DeletionCommand::SpecificModify(string& command) const
{ }


// ===> TrivialErrorNullification <===
// A tool class for effacing trivial errors from MySQLInterface
#include <boost/io/ios_state.hpp>
namespace {
  class TrivialErrorNullification {
    ofstream outflow;
    boost::io::ios_all_saver saver;
  public:
    TrivialErrorNullification():
      outflow("/dev/null"),
      saver(cerr)
    {
      if(outflow.is_open())
        cerr.rdbuf(outflow.rdbuf());
    }
  };
}


// ===> OperationTool::MySQLOperator <===
// MySQL modulus used by OperationTool
// Constructor
OperationTool::MySQLOperator::MySQLOperator(const ArgumentParser& p):
  MySQLInterface(),
  parser(p),
  accepted(false)
{ the_mode = success = failure = 0; }

// Destructor
// All operations accepted, or rejected.
OperationTool::MySQLOperator::~MySQLOperator()
{
  if(failure)
  {
    cerr << failure << " operation" << (failure>1?"s ":" ") << "failed." << endl;
    if(!accepted) accepted = false;
  }

  if(accepted && 0 != success)
  {
    cout << "Committing all operations..." << endl;
    TrivialErrorNullification t;
    DirectQuery("commit");
  }
  else if(0 == success)
    cout << "Nothing done..." << endl;
  else
  {
    cerr << "Undo all operations..." << endl;
    TrivialErrorNullification t;
    DirectQuery("rollback");
  }
}

// Initiate the database connection, find the mode, and start a transaction
bool OperationTool::MySQLOperator::InitiateDatabase()
{
  if(!connectMySQL(parser.Server().c_str(), parser.User().c_str(), parser.Passwd().c_str(), parser.Database().c_str(), parser.Port()))
  {
    cerr << "Connection failed!" << endl;
    return false;
  }

  the_mode = parser.OperationMode();
  if(ArgumentParser::insertion_mode > the_mode ||
     ArgumentParser::deletion_mode < the_mode)
  {
    cerr << "Invalid operation mode: " << the_mode << endl;
    return false;
  }
  else if(ArgumentParser::deletion_mode == the_mode)
    real_method = &MySQLOperator::Delete;
  else if(ArgumentParser::updating_mode == the_mode)
    real_method = &MySQLOperator::Update;
  else 	// ArgumentParser::insertion_mode == the_mode
    real_method = &MySQLOperator::Insert;

  TrivialErrorNullification t;
  DirectQuery("set autocommit = 0");
  DirectQuery("start transaction");
  return true;
}

// Create the instance of insert/update/delete
OperationTool::BaseCommand* OperationTool::MySQLOperator::Generate(const vector<int>& indexes) const
{
  if(indexes.empty())
  {
    cerr << "No columns selected..." << endl;
    return NULL;
  }

  if(ArgumentParser::insertion_mode == the_mode)
    return new OperationTool::InsertionCommand(parser, indexes);
  else if(ArgumentParser::updating_mode == the_mode)
    return new OperationTool::UpdatingCommand(parser, indexes);
  else if(ArgumentParser::deletion_mode == the_mode)
    return new OperationTool::DeletionCommand(parser, indexes);
  else
  {
    cerr << "Unrecognized mode |" << the_mode << "|..." << endl;
    return NULL;
  }
}

// Real query counting
void OperationTool::MySQLOperator::OneRecord(const std::string& the_command)
{
  ++failure;

  if(the_command.empty())
    cerr << "No command acquired..." << endl;
  else if(!(this->*real_method)(the_command))
    cerr << "MySQL query failed on |" << the_command << "|..." << endl;
  else
  {
    --failure;
    ++success;
  }
}


// ===> OperationTool <===
// Construction
OperationTool::OperationTool(const RecordsHolder& h):
  holder(h)
{}

// Authenticating. One needs the key to run OperationTool
bool OperationTool::Authentication() const
{
  ifstream in(Configuration::GetInstance().Authentication());
  if(!in.is_open())
    return false;
  string line;
  getline(in, line);
  return Configuration::GetInstance().Authenticating(line);
}

// Running
bool OperationTool::Run(const ArgumentParser& parser)
{
  size_t N_records = holder.N_records();	// Number of records
  if(0 == N_records)
  {
    cerr << "No records acquired..." << endl;
    return false;
  }

  size_t N_columns = parser.N_Columns();	// Number of columns
  if(0 == N_columns)
  {
    cerr << "No columns available..." << endl;
    return false;
  }

  // In case that some columns are to be removed
  vector<int> removing(N_columns);
  // Container of all records. Each element as a record, each element of a
  // record as an aspect of it.
  vector<vector<string> > all_data;
  all_data.reserve(N_records);
  for(size_t i=0;i<N_records;++i)
  {
    vector<string> one_record;
    holder.GetRecord(i, one_record);
    for(size_t j=0;j<N_columns;++j)
    {
      if(one_record[j].empty())
      {
        ++removing[j];		// NULL detected
//      one_record[j].assign("NULL");
      }
    }
    all_data.push_back(one_record);
  }

  vector<int> indexes;	// Indexes of columns whose contents are not NULL
  indexes.reserve(N_columns);
  for(size_t i=0;i<N_columns;++i)
  {
    if(0 == removing[i])	// No NULL contents
      indexes.push_back(i);
  }
  if(indexes.size() != N_columns)
  {
    if(ArgumentParser::json_file == parser.TerminalMode())
      cout << "Trivial column detected..." << endl;
    N_columns = indexes.size();
  }
  if(0 == N_columns)
  {
    cout << "All columns are trivial. Nothing to do." << endl;
    return true;
  }

  // Create MySQLOperator for OperationTool and initiate the database
  MySQLOperator manipulater(parser);
  if(!manipulater.InitiateDatabase())
  {
    cerr << "Can't initiate MySQL handler..." << endl;
    return false;
  }

  // Instantiate the real operator from insert/update/delete
  BaseCommand* command_generator = manipulater.Generate(indexes);
  if(!command_generator)
  {
    cerr << "FATAL ERROR!!! CAN'T GENERATE MYSQL QUERY COMMANDS!!!" << endl;
    return false;
  }
  else if(!command_generator->InitiateDatabase())
  {
    cerr << "Initiating MySQL command generator failed." << endl;
    return false;
  }

  // Ready to handle each record
  if(indexes.size() == parser.N_Columns())
  {
    for(size_t i=0;i<N_records;++i)
    {
      try {
        manipulater.OneRecord(command_generator->GetCommand(all_data[i]));
      } catch(BadCommand& e) {
        cout << "Warning... Can't generate command on " << i << "..." << endl;
        manipulater.FailedRecord();
      }
    }
  }
  else
  {
    for(size_t i=0;i<N_records;++i)
    {
      vector<string> a_record;
      const vector<string>& one_record = all_data[i];
      for(size_t j=0;j<N_columns;++j)
        a_record.push_back(one_record[indexes[j]]);
      try {
//      string this_record(command_generator->GetCommand(a_record);
//      manipulater.OneRecord(this_record);
        manipulater.OneRecord(command_generator->GetCommand(all_data[i]));
      } catch(BadCommand& e) {
        cout << "Warning... Can't generate command on " << i << "..." << endl;
        manipulater.FailedRecord();
      }
    }
  }

  delete command_generator;
  // If it gets here, ready to commit the modification.
  manipulater.Accepted();

  return true;
}
