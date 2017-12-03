// ************************************************************************* //
// File: common_tools.cc
// Contents: common functions that both tools will use
// Classes:
//   MySQLInterface
//   BaseParser::ColumnAcquirer
//   BaseParser
//   RecordsHolder
// ************************************************************************* //
#include "common_tools.h"
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <algorithm>
#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>	// For getenv
using namespace std;

const char* const INSTALLATION_FOLDER = "---";
const char* const CONFIGURATION_FOLDER = "./";

// ===> MySQLInterface <===
// MySQL interface. Handles all MySQL query
// Constructor: initialize variables and connect to database
MySQLInterface::MySQLInterface():
    rows(-1), fields(-1),
    errorNum(0), errorInfo("ok"),
    result(0)
{
  mysql_init(&mysqlInstance);
}

// Destructor (close the connection)
MySQLInterface::~MySQLInterface()
{
//cerr<<"Closing MYSQL connection."<<endl;
  closeMySQL();
//cerr<<"MySQLInterface destructed."<<endl;
}

// Connect to MYSQL server
// Need 5 arguments (but last 4 with default values):
//   1. server (IP address)
//   2. Account to log in the server
//   3. Password of the account
//   4. Database to use
//   5. Port
// Returns true if connected, false otherwise
bool MySQLInterface::connectMySQL(const char* server, const char* username, const char* password, const char* database, int port)
{
  if(::mysql_real_connect(&mysqlInstance,server,username,password,database,port,0,0))
    return true;
  else
  {
    errorIntoMySQL();
    return false;
  }
}

// Data retriever
// Need two arguments:
//   1. MySQL query
//   2. vector of vector of string. Saves what is retrieved from database
// Returns true if nothing wrong
bool MySQLInterface::GetDataFromDB(const string& queryStr, vector<vector<string> >& data)
{
  // Empty string check
  if(queryStr.empty())
  {
    cerr<<"No command to query."<<endl;
    return false;
  }
  else if(!TestingQueryCommand(queryStr, "select"))
  {
    // If <queryStr> doesn't start with "select"
    cerr << "Illegal query: command shall start with \"insert\"." << endl;
    return false;
  }

  // Real query
  int res = ::mysql_real_query(&mysqlInstance,queryStr.c_str(),queryStr.size());
  if(res)
  {
    cerr<<"Query |\033[3m"<<queryStr<<"\033[0m| failed."<<endl;
    this->errorIntoMySQL();
    return false;
  }

  // Clear the buffer here
  ClearData();
  // Retrieve info from database
  result = ::mysql_store_result(&mysqlInstance);
  if(!result)
  {
    cerr<<"No result retrieved from database."<<endl;
    return false;
  }

  // Update the number of rows and fields of this query
  rows = ::mysql_num_rows(result);
  fields = ::mysql_num_fields(result);
//cerr<<rows<<" rows in set, each with "<<fields<<" columns."<<endl;

  // Remove the content of outer container if any
  if(data.size())
  {
//  cerr<<"Container not empty. Clearing them..."<<endl;
    int NSize = data.size();
    for(int i=0;i<NSize;++i) data[i].clear();
    data.clear();
//  cerr<<"All "<<NSize<<" vector"<<(NSize>1?"s ":" ")<<"cleared."<<endl;
  }
  // Reserve enough space
  data.reserve(rows);

  // Redirect info from within this object to the outer container, i.e., <data>
  for(int i=0;i<rows;++i)
  {
    MYSQL_ROW line = ::mysql_fetch_row(result);
    if(!line)
    {
      cerr<<"No. "<<i+1<<" row is empty."<<endl;
      continue;
    }
    vector<string> linedata;
    linedata.reserve(fields);
    for(int i=0;i<fields;i++)
    {
      if(line[i])
      {
        string temp = line[i];
        linedata.push_back(temp);
      }
      else
      { linedata.push_back(""); }
    }
    data.push_back(linedata);
  }
  return true;
}

// Error messages
void MySQLInterface::errorIntoMySQL()
{
  errorNum  = ::mysql_errno(&mysqlInstance);
  errorInfo = ::mysql_error(&mysqlInstance);
  cerr<<"\033[31mError INFO: "<<errorInfo<<"\033[0m"<<endl;
  cerr<<"\033[31mError code: "<<errorNum<<"\033[0m"<<endl;
}

// Close the connection
void MySQLInterface::closeMySQL()
{
  ClearData();
  ::mysql_close(&mysqlInstance);
  ::mysql_library_end();
}

// Displaying data from MySQL in MySQL's way
// Need 2 arguments:
//   1: vector of vector of string of contents from mysql query. Inner vectors
//      shall have identical number of elements.
//   2: name of columns in the query. Can be empty, and thus not displayed.
void MySQLInterface::PrintingValues(const vector<vector<string> >& data, const vector<string>& names_of_column)
{
  int Length = data.size();
  if(0 == Length)
  {
    cout<<"No value to print."<<endl;
    return;
  }
  int NSize = data.front().size();
  // Maximal width for each column. Used later.
  vector<int> width(NSize, 0);
  // Find the width of each column (maximal width of each row plus 2)
  if(!names_of_column.empty())
  {
    // If names of columns are given
    if(names_of_column.size() != NSize)
    {
      // Names and values don't match
      cerr<<"\033[31mFATAL ERROR!!! Names of columns and their values don't match!!!\033[0m"<<endl;
      cerr<<"\033[31mNumber of columns: "<<names_of_column.size()<<"   that of values: "<<NSize<<"\033[0m"<<endl;
      throw runtime_error("Unmatched columns");
    }
    // Initiate the width by each title
    for(int i=0;i<NSize;++i)
      width[i] = names_of_column[i].size();
  }
//cout<<"Size of one set: "<<NSize<<endl;
  // Loop every record and find the maximal width for each column
  for(int i=0;i<Length;++i)
  {
    const vector<string>& one_set = data.at(i);
    int one_set_size = one_set.size();
    if(one_set_size != NSize)
    {
      cout<<"\033[33mFATAL ERROR... Size of No. "<<i+1<<" ("<<one_set_size<<") differs from that of the first ("<<NSize<<")..."<<endl;
      throw;
    }
    for(int j=0;j<one_set_size;++j)
    {
      const string& one_element = one_set[j];
      if(one_element.empty() && 4 < width[j]) width[j] = 4;
      else if(one_element.size() > width[j]) width[j] = one_element.size();
    }
  }
  // 1 for a whitespace, leading one. The tailing one is added later
  for(int i=0;i<NSize;++i) width[i] += 1;
  string concatenation("+"); // Border
  vector<string> on_screen; // Content to be displayed; serves as a buffer

  // Length: one line for each set
  // 2: border on the top and bottom
  // names_of_column staff: top border and names of columns
  on_screen.reserve(Length+2+(int(!names_of_column.empty()))*2);

  // Prepare for the border
  for(int i=0;i<NSize;++i)
  {
    // This 1 is the tailing one
    concatenation.append(width[i]+1, '-');
    concatenation.append("+");
  }
  // Add top border
  on_screen.push_back(concatenation);

  if(!names_of_column.empty())
  {
    // Add titles and a second border
    on_screen.push_back("| ");
    for(int i=0;i<NSize;++i)
    {
      on_screen[1] += names_of_column[i];
      on_screen[1].append(width[i]-names_of_column[i].size(), ' ');
      on_screen[1].append("| ");
    }
    on_screen.push_back(concatenation);
  }
  // Join each set and insert into the buffer
  for(int i=0;i<Length;++i)
  {
    string one_line("| ");
    const vector<string>& one_set = data[i];
    for(int j=0;j<NSize;++j)
    {
      string one_element = one_set[j];
      // For empty elements
      if(one_element.empty()) one_element = "NULL";
      one_line += one_element;
      one_line.append(width[j]-one_element.size(), ' ');
      one_line.append("| ");
    }
    on_screen.push_back(one_line);
  }
  // Add bottom border
  on_screen.push_back(concatenation);

  // Displaying
  Length = on_screen.size();
  for(int i=0;i<Length;++i)
    cout<<on_screen[i]<<endl;
  Length = data.size();
  cout << Length << (Length>1?" rows":" row") << " in set." << endl;
}

// Print name of columns of last query (selection), but only their names
void MySQLInterface::PrintNameOfColumns() const
{
  vector<string> names;
  if(!this->GenerateNameOfColumns(names))
  {
    cerr<<"No names of columns found."<<endl;
    return;
  }

  int NNames = names.size();
  if(NNames != fields)
  { cerr<<"Warning... "<<NNames<<" columns don't match that acuqired before ("<<fields<<")..."<<endl; }

  cerr<<"Printing "<<NNames<<" columns of this query."<<endl;
  for(int i=0;i<NNames;++i)
  {
    const string& one_name = names.at(i);
    if(one_name.empty())
      cerr<<"\033[31mColumn "<<i+1<<" is unavailable.\033[0m"<<endl;
    else
      cerr<<"Column "<<i+1<<": "<<one_name<<endl;
  }
  cerr<<"Columns printed.\n"<<endl;
}

// Retrieve names of columns of last selection
bool MySQLInterface::GetNameOfColumns(vector<string>& names) const
{ return this->GenerateNameOfColumns(names); }

// Deduce names of columns of last selection
bool MySQLInterface::GenerateNameOfColumns(vector<string>& names) const
{
  names.clear();
  if(!result)
  {
    // No result.
    cerr<<"Query failed."<<endl;
    return false;
  }
  if(-1 == fields)
  {
    // No columns acquired, due to no query at all or failed queries, etc.
    cerr<<"No column acquired."<<endl;
    return false;
  }

  // Fetch the names of columns
  for(int i=0;i<fields;i++)
  {
    MYSQL_FIELD *fd = ::mysql_fetch_field(result);
    if(fd)
      names.push_back(fd->name);
    else
      names.push_back("");
  }

  return bool(names.size());
}

// Performing non-selection query of MySQL
// Need 1 argument:
//   1. MySQL query command
// (Not suggested to be called directly)
bool MySQLInterface::Non_Select_Query(const string& queryStr)
{
  if(queryStr.empty())
  {
    cerr << "Empty non-select query command..." << endl;
    return false;
  }

  int result = ::mysql_real_query(&mysqlInstance, queryStr.c_str(), queryStr.size());
  if(result)
  {
    cerr << "Error code: " << result << endl;
    return false;
  }

  rows = ::mysql_affected_rows(&mysqlInstance);
  return true;
}

// Called by non-select query; testing if a query command matches
// Need 2 arguments:
//   1. Query command
//   2. Expected type of command, a word, that shall be found in the command
// Returns true if matches, false otherwise
bool MySQLInterface::TestingQueryCommand(const string& queryStr, const string& expected)
{
  if(queryStr.empty())
  {
    cerr << "Empty command..." << endl;
    return false;
  }

  string to_split(queryStr);
  if(';' == queryStr[0])
  {
    // To remove leading semicolon(s) in the <queryStr>
    const char* command = queryStr.c_str();
    int counts = 0;
    while(';' == *command) { ++command; ++counts; }
    to_split = to_split.substr(counts);
    boost::trim_left(to_split);
  }
  // Use whitespace to split <queryStr> (no leading ";" version)
  vector<string> holder;
  boost::split(holder, to_split, boost::is_space());

  if(holder.empty()) return false;

  if(expected == boost::to_lower_copy(holder[0]))
    return true;
  else
    return false;
}

bool MySQLInterface::Insert(const string& queryStr)
{
  if(!TestingQueryCommand(queryStr, "insert"))
  {
    cerr << "Illegal query: command shall start with \"insert\"." << endl;
    return false;
  }

  return Non_Select_Query(queryStr);
}

bool MySQLInterface::Update(const string& queryStr)
{
  if(!TestingQueryCommand(queryStr, "update"))
  {
    cerr << "Illegal query: command shall start with \"update\"." << endl;
    return false;
  }

  return Non_Select_Query(queryStr);
}

bool MySQLInterface::Delete(const string& queryStr)
{
  if(!TestingQueryCommand(queryStr, "delete"))
  {
    cerr << "Illegal query: command shall start with \"delete\"." << endl;
    return false;
  }

  return Non_Select_Query(queryStr);
}

// Direct query. Should be used only when necessary
bool MySQLInterface::DirectQuery(const string& queryStr)
{
  cerr << "Warning... Direct query (no result to fetch) called..." << endl;
  return Non_Select_Query(queryStr);
}


// ===> BaseParser::ColumnAcquirer <===
// Prepare the structure of table for BaseParser
// Constructor
BaseParser::ColumnAcquirer::ColumnAcquirer()
{
  names.reserve(32);
  levels.reserve(16);
}

// Retrieve the structure of the target table from database
// Need 4 arguments:
//   1. server
//   2. database
//   3. port
//   4. name of table
// Returns true if retrieved, false otherwise
bool BaseParser::ColumnAcquirer::Acquire(const string& server, const string& database, int port, const string& table)
{
  // Uses default account to log in
  if(!connectMySQL(server.c_str(),
                   Account::default_account.c_str(),
                   Account::default_password.c_str(),
                   database.c_str(),
                   port
                  )
    )
  {
    cerr << "Error connecting while getting columns..." << endl;
    return false;
  }

  if(table.empty())
  {
    cout << "NULL TABLE..." << endl;
    return false;
  }

  string describing("describe ");
  describing.append(table);
  // describing: describe <table name>
  vector<vector<string> > data;
  if(!GetDataFromDB(describing, data))
  {
    cout << "Can't retrieve the structure..." << endl;
    return false;
  }
  else if(data[0].empty())
  {
    cout << "Nothing available for the table " << table << endl;
    return false;
  }
  else if(data[0].size() < 4)
  {
    cout << "Error element scale |" << data[0].size() << "|." << endl;
    return false;
  }

  size_t s = data.size();
  names.resize(s*2);
  // [0] - [s-1]: the name of columns
  // [s] - [s+s-1]: the type of the columns
  map<string, size_t> index_of_column;
  for(size_t i=0;i<s;++i)
  {
    names[i].assign(data[i][0]);
    names[i+s].assign(data[i][1]);
    data[i].clear();
    index_of_column.insert(make_pair(data[i][0], i));
  }

  // Clearing the buffer. Prepare the object for printing levels from comments
  // of each column.
  data.clear();
  ClearData();
  describing.clear();
  describing.assign("select column_name Name, column_comment comment from information_schema.columns where table_name = '");
  describing.append(table);
  describing.append("' and table_schema = '");
  describing.append(database);
  describing.append("'");
  if(!GetDataFromDB(describing, data))
  {
    cerr << "Can't retrieve printing levels..." << endl;
    return false;
  }
  else if(data.size() != s)
  {
    cerr << "Warning... Size of printing levels doesn't match (" <<
            data.size() << ", " << s << ")..." << endl;
    return false;
  }
  for(size_t i=0;i<s;++i)
  {
    const vector<string>& one_column = data[i];
    if(index_of_column.end() == index_of_column.find(one_column[0]))
    {
      cout << "Warning... No " << one_column[0] << " found..." << endl;
      continue;
    }
    string level = one_column[1];
    // Comment of a column, if multiple properties, uses "|" to separate them
    if(string::npos != level.find("|"))
    {
      vector<string> levels;
      boost::split(levels, level, boost::is_any_of("|"));
      int found = -1;
      for(int i=0;i<levels.size();++i)
      {
        // Hint of printing level is either "PL" or "PrintingLevel"
        if(string::npos == levels[i].find("PL") &&
           string::npos == levels[i].find("PrintingLevel"))
          continue;
        if(-1 != found)
          cout << "Warning... Multiple PrintingLevel detected... Overwriting (" << found << " => " << i << ")..." << endl;
        found = i;
      }
      if(-1 == found)
      {
        cout << "Warning... Unrecognized printing level for |" <<
                one_column[0] << "|: " << level << endl;
        continue;
      }
      else
        level.assign(levels[found]);
    }

    if(string::npos != level.find("PL"))
      level.erase(0, level.find("PL") + 2);
    else if(string::npos != level.find("PrintingLevel"))
      level.erase(0, level.find("PrintingLevel") + 13);
    else
    {
      cout << "Warning... Unrecognized printing level for " <<
              one_column[0] << ": " << level << endl;
      continue;
    }
    levels.push_back(atoi(level.c_str()));
  }
  return true;
}

// Deliver structure of table
// Need two arguments:
//   1. outer container of names of columns
//   2. outer container of printing levels of each column
bool BaseParser::ColumnAcquirer::Deliver(vector<string>& o_names, vector<int>& o_levels) const
{
  if(!o_names.empty())
    o_names.clear();
  if(!o_levels.empty())
    o_levels.clear();

  size_t scale = names.size() / 2;

  if(0 == scale)
  {
    cout << "Nothing to transfer..." << endl;
    return false;
  }

  o_names.resize(scale);
  o_levels.resize(scale);
  for(size_t i=0;i<scale;++i)
  {
    o_names[i].assign(names[i]);
    o_levels[i] = levels[i];
  }

  if(o_names.empty())
  {
    cerr << "Error! No column retrieved." << endl;
    return false;
  }
  else
    return true;
}


// ===> BaseParser <===
// Base class of SelectionTool and OperationTool
// Constructor
BaseParser::BaseParser():
  trivial_account("trivial_account"),	// Trivial account
  trivial_password("to be replaced")	// Trivial password
{
  port = 3306;
  number_of_columns = 0;
}

// Destructor
BaseParser::~BaseParser()
{}

// General components assembler and some common properties both tools share
// Need 1 argument:
//   1. option descriptions (BOOST_PROGRAM_OPTIONS)
void BaseParser::GeneralAssembler(boost::program_options::options_description& opts) const throw(runtime_error)
{
  if(!opts.options().empty())
  {
    cerr << "FATAL ERROR!!!" << endl;
    cerr << "There are option(s) in the description:" << endl;
    cerr << opts << endl;
    cerr << "This method shall be called at first!" << endl;
    throw runtime_error("MYSQL FAILED");
  }

  using boost::program_options::value;

  // Add common options
  opts.add_options()
    ("server,S", value<string>(), "Server of database")
    ("user,U", value<string>()->default_value(trivial_account), "User account")
    ("passwd,W", value<string>()->default_value(trivial_password), "Password for that user to login")
    ("database,D", value<string>(), "Name of Database")
    ("port,P", value<int>()->default_value(3306), "Port of connection")
    ("options,o", value<string>(), "Options for the query")
    ("type,T", value<string>(), "Type of records concerned")
    ("help,h", "Print help message and exit")
    ("manual,m", "Print detailed help message and exit")
  ;
}

// Get general parameters that either tool will use
// Need 2 argument:
//   1. variables_map (BOOST_PROGRAM_OPTIONS)
//   2. print level as priority (not used yet)
void BaseParser::GetGeneralParameters(const boost::program_options::variables_map& v_map, int level) throw(runtime_error)
{
  try {
    // To parse the configuration file if any
    if(v_map.count("options"))
      IngestOptions(v_map["options"].as<string>());

    // Replace the default value of general parameters with real ones from
    // terminal arguments if it is given.
    for(int i=0;i<5;++i)
    {
      if(2 == i) continue;
      if(v_map.count(general_parameter_names[i]))
        general_parameters[i].assign(v_map[general_parameter_names[i]].as<string>());
    }
    // Special reaction for password. A trivial password is used if none given.
    if(v_map.count(general_parameter_names[2]))
    {
      string password(v_map[general_parameter_names[2]].as<string>());
      if(trivial_password != password)
        general_parameters[2].assign(password);
    }
    // For account & password. First, the account
    if(Account::default_account == general_parameters[1])
    {
      // If default_account is used, make sure the password also matches 
      // No reverse matching from password to account is committed here.
      if(Account::default_password != general_parameters[2])
        general_parameters[2].assign(Account::default_password);
    }
    else
    {
      // In case of non-default account. If no input for password, it is still
      // the default one, but default password is trivial and should thus be
      // acquired interactively
      if(trivial_password == general_parameters[2])
        InteractivePassword();
    }

    if(v_map.count("port"))
      port = v_map["port"].as<int>();
  } catch(exception& e) {
    throw runtime_error(e.what());
  }

  if(!OnDefaultAccounts())
  {
    // The default account doesn't make it (rejected by OperationTool)
    throw runtime_error("Default account rejected");
  }

  // Check if all general parameters are ready.
  for(int i=0;i<5;++i)
  {
    if(general_parameters[i].empty())
      throw runtime_error(" unavailable " + general_parameter_names[i]);
  }
  // Checking for repulsive options
  string repulsive(ReplusiveOptions(v_map));
  if(!repulsive.empty())
    throw runtime_error(repulsive);

  try {
    PrepareColumns();
  } catch(runtime_error& e) {
    throw;
  }
}

// Interactively take in the password
void BaseParser::InteractivePassword()
{
  cout << "Enter password:";
  cout.flush();
  string password;
  cin >> password;
  if(!password.empty() && trivial_password != password)
    general_parameters[2].assign(password);
  else
    general_parameters[2].clear();
}

#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
// Use DNS to convert the domain name of the server into its IP address
void BaseParser::AcquiringRealServer()
{
  if(general_parameters[0].empty())
  {
    cerr << "No hint for server available..." << endl;
    return;
  }

  int expected = general_parameters[0][0] - '0';
  if(0 <= expected && 3 > expected)	// the first char is 0, 1 or 2:
    return;				// all numbers assumed

  string ptr = general_parameters[0];
  struct hostent *hptr = gethostbyname(ptr.c_str());
  if(hptr == NULL)
  {
    cerr << "gethostbyname error: |" << ptr << "|." << endl;
    return;
  }

  if(AF_INET == hptr->h_addrtype || AF_INET6 == hptr->h_addrtype)
  {
    char str[32] = {0};
    general_parameters[0].assign(
        inet_ntop(hptr->h_addrtype, hptr->h_addr, str, sizeof(str))
    );
  }
  else
    general_parameters[0].clear();
}

// Parse the option file acquired by terminal arguments
void BaseParser::IngestOptions(string option_file)
{
  if(option_file.empty())
  {
    cout << "NULL name for options..." << endl;
    return;
  }

  int open_state = GuessOptionFileName(option_file);
  if(NoSuchFile == open_state)
  {
    cerr << "Can't locate configuration file..." << endl;
    return;
  }
  else if(IllegalFile == open_state)
  {
    cout << "Can't open option file \"" << option_file << "\"..." << endl;
    return;
  }

  ifstream in(option_file.c_str());
  if(!in.is_open())
  {
    cout << "Can't open configuration file |" << option_file << "|..." << endl;
    return;
  }

  string line;
  int N_parameter_names = sizeof(general_parameter_names)/sizeof(general_parameter_names[0]);
  // Trying to ingest the configuration file. A line after another.
  while(getline(in, line))
  {
    if(3 > line.size())
      continue;
    if(string::npos == line.find("="))
      continue;
    size_t equal = line.find("=");
    string options = line.substr(0, equal);
    boost::to_lower(options);
    int index = find(general_parameter_names, general_parameter_names+N_parameter_names, options) - general_parameter_names;
    if(N_parameter_names == index)
      continue;
    else if(5 == index)
      port = atoi(line.substr(equal+1).c_str());
    else if(0 <= index && 5 > index)
      general_parameters[index] = line.substr(equal+1);
    else
      cout << "Error parsing " << line << ": (" << index << ", " << equal << ", " << options << ")." << endl;
  }
}

// Guess name of option file.
// Returns 0 if valid name, non-0 otherwise.
int BaseParser::GuessOptionFileName(string& option_file) const
{
  // First, if option_file is good, returns directly.
  ifstream in(option_file.c_str());
  if(in.is_open())
    return SuccessfulAccess;

  string basename(option_file);
  if(string::npos != option_file.find("/"))
  {
    // Guessing option_file's path is unnecessary
    basename.erase(0, basename.find("/")+1);
    if(basename.empty())
      return NoSuchFile;
    in.open(basename.c_str());
    if(in.is_open())
    {
      cout << "Configuration file in the current folder selected." << endl;
      option_file.assign(basename);
      return SuccessfulAccess;
    }
  }

  // Try to locate the option in one's HOME
  string new_option_file(getenv("HOME"));
  if(boost::ends_with(new_option_file, "/"))
    new_option_file.append("/");
  new_option_file.append(basename);
  in.open(new_option_file.c_str());
  if(in.is_open())
  {
    cout << "Configuration file in HOME selected." << endl;
    option_file.assign(new_option_file);
    return SuccessfulAccess;
  }

  // Try to locate the option in the installation prefix
  string installation_folder(Configuration::GetInstance().InstallationFolder());
  if(string::npos == installation_folder.find("---"))
  {
    new_option_file.assign(installation_folder);
    if(boost::ends_with(new_option_file, "/"))
      new_option_file.append("/");
    new_option_file.append(basename);
    in.open(new_option_file.c_str());
    if(in.is_open())
    {
      cout << "Configuration file in installation folder selected." << endl;
      option_file.assign(new_option_file);
      return SuccessfulAccess;
    }
  }

  // Try to locate the option in ConfigurationFolder from Configuration
  new_option_file.assign(Configuration::GetInstance().ConfigurationFolder());
  if(boost::ends_with(new_option_file, "/"))
    new_option_file.append("/");
  new_option_file.append(basename);
  in.open(new_option_file.c_str());
  if(in.is_open())
  {
    cout << "Configuration file in installation folder selected." << endl;
    option_file.assign(new_option_file);
    return SuccessfulAccess;
  }

  return IllegalFile;
}

// Testing if replusive options co-exist
// Need 1 argument:
//   1. variables_map (BOOST_PROGRAM_OPTIONS)
// Returns the replusive options given (empty string if none)
string BaseParser::ReplusiveOptions(const boost::program_options::variables_map& v_map)
{
  size_t N_repulsives = repulsive_options.size();
  if(0 == N_repulsives)
    return "";

  string repulsives;
  int counts = 0;
  for(size_t i=0;i<N_repulsives;i+=2)
  {
    if(v_map.count(repulsive_options[i]) && v_map.count(repulsive_options[i+1]))
    {
      repulsives.append("|");
      repulsives.append(repulsive_options[i]);
      repulsives.append(" <==> ");
      repulsives.append(repulsive_options[i+1]);
      repulsives.append("|; ");
      ++counts;
    }
  }

  if(counts)
  {
    cout << counts << (1<counts?" pairs":" pair") << "Replusive options detected! " << endl;
    repulsives.erase(repulsives.size() - 2);
  }
  return repulsives;
}

// Preparation. Load columns, find names and levels.
void BaseParser::PrepareColumns() throw(runtime_error)
{
  // general_parameters[3]: database
  if(general_parameters[3].empty())
  {
    cerr << "NO database to query..." << endl;
    throw runtime_error("NULL DATABASE");
  }

  ColumnAcquirer tool;
  if(!tool.Acquire(general_parameters[0], general_parameters[3], port, general_parameters[4]))
  {
    cerr << "Can't load the columns..." << endl;
    throw runtime_error("NO COLUMN");
  }

  // In the calling below, three arguments are all updated
  if(!tool.Deliver(name_of_columns, level_of_columns))
    throw runtime_error("No column received");

  // Update the value of number_of_columns
  number_of_columns = name_of_columns.size();
  // In case of mismatch...
  if(level_of_columns.size() != number_of_columns)
  {
    ostringstream os;
    os << "Names and levels don't match: " << number_of_columns;
    os << " <===> " << level_of_columns.size();
    throw runtime_error(os.str());
  }
}

size_t BaseParser::N_Records() const
{ return 200; }

// Retrieve names of columns
// Need 2 arguments:
//   1. outer container
//   2. print level as priority (not used yet)
void BaseParser::Columns(vector<string>& target, int level) const
{
  if(!target.empty())
    target.clear();

  size_t scale = name_of_columns.size();
  target.reserve(scale);
  if(0 == scale)
    return;

  for(size_t i=0;i<scale;++i)
    target.push_back(name_of_columns[i]);
}

// Common options
const string BaseParser::general_parameter_names[] = { "server", "user", "passwd", "database", "type", "port" };


// ===> RecordsHolder <===
// Container of records
// Constructor
RecordsHolder::RecordsHolder():
  records(),
  first_column(),
  number_of_columns(0),
  number_of_records(0),
  name_of_columns()
{}

// Ingest the common info for RecordsHolder from terminal via BaseParser
bool RecordsHolder::Initialize(const BaseParser& parser)
{
  if(parser.NotReady())
    return false;

  Reserve(parser.N_Records());
  parser.Columns(name_of_columns);
  number_of_columns = name_of_columns.size();

  return true;
}

// Insert a record, elements of vector being each column
void RecordsHolder::Insert(const vector<string>& record, int index_of_lfn)
{
  if(record.empty())
  {
    cerr << "NULL RECORD..." << endl;
    return;
  }
  else if(-1 == index_of_lfn)
  {
    cerr << "No index of LFN received..." << endl;
    return;
  }

  string token;
  // If index_of_lfn == -2, the caller of this method doesn't know where lfn is
  // so it requires a search locally.
  if(-2 == index_of_lfn)
    index_of_lfn = FindLFN();

  // After the search, or if -1 is passed from outside, it means no lfn is
  // found. Use the first two aspects.
  if(-1 == index_of_lfn)
  {
    token.assign(record.front());
    if(record.size() > 1)
    {
      token.append("|");
      token.append(record[1]);
    }
  }
  else if(record.size() <= index_of_lfn)
  {
    // In case of invalid index of lfn
    cerr << "Invalid index of LFN received: " << index_of_lfn << " (0-" << record.size() << ")." << endl;
    return;
  }
  else
    token.assign(record[index_of_lfn]);

  if(first_column.end() != find(first_column.begin(), first_column.end(), token))
  {
    // Inserted records are omitted.
    cerr << "This record has been inserted..." << endl;
    cerr << "Telling token: |" << token << "|." << endl;
    return;
  }

  records.push_back(record);
  first_column.push_back(token);
  ++number_of_records;
}

// Insert a record, with all columns joined by one char in <separator>
void RecordsHolder::Insert(const string& record, const string& separator)
{
  if(record.empty())
  {
    cerr << "NULL RECORD..." << endl;
    return;
  }

  vector<string> splitted;
  boost::split(splitted, record, boost::is_any_of(separator.c_str()), boost::token_compress_on);
  Insert(splitted);
}

// Retrieve a record with a vector. Empty vector results if wrong <location>
void RecordsHolder::GetRecord(size_t location, vector<string>& target) const
{
  if(!target.empty())
    target.clear();

  if(!ValidLocation(location))
    return;

  target.reserve(N_columns());
  copy(records[location].begin(), records[location].end(), back_inserter(target));
}

// Retrieve a record with a string joining all columns with a ';'. Empty
// string results if wrong <location>
string RecordsHolder::GetRecord(size_t location) const
{
  if(!ValidLocation(location))
    return "";

  const vector<string>& wanted = records[location];
  if(wanted.empty())
    return "";
  else
    return boost::join(wanted, ";");
}

void RecordsHolder::Print() const
{
  MySQLInterface::PrintingValues(records, name_of_columns);
}

#include <boost/io/ios_state.hpp>
void RecordsHolder::Print(ostream& outflow) const
{
  boost::io::ios_all_saver saver(cout);
  cout.rdbuf(outflow.rdbuf());
  MySQLInterface::PrintingValues(records, name_of_columns);
}

// Find the index of Logical File Name from name_of_columns
int RecordsHolder::FindLFN() const
{
  if(name_of_columns.empty())
    return -1;

  int index_of_lfn = -1;
  vector<string>::const_iterator it = find(name_of_columns.begin(), name_of_columns.end(), "lfn");
  if(name_of_columns.end() != it)
    index_of_lfn = it - name_of_columns.begin();
  return index_of_lfn;
}
