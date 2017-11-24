// ************************************************************************* //
// File: selection_tool.cc
// Contents: modules used by SelectionTool
// Classes:
//   Conditions::BadGeneration
//   Conditions::MapInitiater
//   Conditions
//   ArgumentParser
//   FactoryInitializer
//   SelectionTool
// ************************************************************************* //
#include "selection_tool.h"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <ctime>
using namespace std;

namespace {
  class Replacing {
  public:
    void operator()(string& one_column)
    {
      if(one_column.empty())
        one_column.assign("NULL");
    }
  };
}

// ===> Conditions::BadGeneration <===
// Exception class used in case of bad generation
// Common constructor
Conditions::BadGeneration::BadGeneration(const string& hint):
  logic_error(hint)
{}


// ===> Conditions::MapInitiater <===
// Initiate the static map of Conditions
Conditions::MapInitiater::MapInitiater()
{
  map<string, string_processer>& that_map = Conditions::function_pool;
  that_map.insert(make_pair("size", &Conditions::ProcessSize));
  that_map.insert(make_pair("last-modified", &Conditions::ProcessDate));
}

// Initiater
void Conditions::MapInitiater::InitiateMap()
{
  static MapInitiater instance;
}


// ===> Conditions <===
// Manager of MySQL selection conditions. An instance, a condition. It
// measures the range of a column.
// Four types of conditions are supported (achieved by):
//   * value > low (high: UNLIMITED)
//   * value < high (low: UNLIMITED)
//   * low < value < high
//   * value = fixed_value (low == high == fixed_value)
// Constructor (v1)
Conditions::Conditions():
  state_of_this_class(false),
  title(),
  low_edge(),
  high_edge()
{}

// Constructor (v2)
Conditions::Conditions(const string& name, const string& low, const string& high):
  state_of_this_class(false),
  title(name),
  low_edge(),
  high_edge()
{
  AddQuote(low, low_edge);
  if("UNAVAILABLE" == high)
    AddQuote(low, high_edge);
  else
    AddQuote(high, high_edge);
  ValidateCondition();
}

Conditions::Conditions(const string& name, double low, double high):
  state_of_this_class(false),
  title(name)
{
  Convert(low, low_edge);
  if(-5e8 > high)
    Convert(low, high_edge);
  else
    Convert(high, high_edge);

  ValidateCondition();
}

// Make sure this instance is ready, or if it can't, return false
bool Conditions::ValidateCondition()
{
  if(state_of_this_class)
    state_of_this_class = false;

  if(!ValidCondition())
    return false;

  if(low_edge > high_edge)
    std::swap(low_edge, high_edge);

  if(Conditions::function_pool.end() != Conditions::function_pool.find(title))
  {
    string_processer processor = Conditions::function_pool[title];
    (this->*processor)(low_edge);
    (this->*processor)(high_edge);
  }

  return state_of_this_class = true;
}

// Processors of contents of conditions
// To convert the unit of size from MB to B
void Conditions::ProcessSize(std::string& target_string)
{
  double target_v = atof(target_string.c_str());
  target_v *= (1 << 20);
  ostringstream os;
  os << target_v;
  target_string.assign(os.str());
}
// To convert the format of date to YYYYmmDD-HHMMSS
void Conditions::ProcessDate(string& target_date)
{
  // Acceptable input format:
  //   YYYYmmDD
  //   HHMMSS
  //   [[YYYY]mm]DD_HH[MM[SS]]
  string initial(target_date);
  boost::erase_all(target_date, "'");
  size_t N_chars = target_date.size();
  if(boost::all(target_date, boost::is_digit()))
  {
    // All letters are numbers. Try to complement it if insufficient digits.
    if(8 == N_chars)
      target_date.append("_000000");
    else if(6 >= N_chars && 0 == N_chars%2)
    {
      target_date.insert(0, GetTimeNow().substr(0, 9));
      for(size_t i=6-N_chars;i>0;i+=2)
        target_date.append("00");
    }
    else
    {
      cerr << "Illegal date received: |" << initial << "|(" << target_date << ")(NUMBER). Remove it." << endl;
      target_date.assign(unlimited);
      return;
    }
  }
  else
  {
    // Not all digits are numbers. Change its format.
    boost::replace_all(target_date, "-", "_");
    size_t underline = target_date.find("_");
    if(string::npos == underline)
    {
      cerr << "Illegal date received: |" << initial << "|(" << target_date << ")(UNDERLINE). Remove it." << endl;
      target_date.assign(unlimited);
      return;
    }
    if(15 != N_chars)
    {
      string now(GetTimeNow());
      if(7 != N_chars - underline)
        target_date.append(now.substr(N_chars-underline));
      if(8 != underline)
        target_date.insert(0, now.substr(8-underline));
    }
  }

  N_chars = target_date.size();
  if(15 != N_chars)
  {
    cerr << "Illegal date received: |" << initial << "|(" << target_date << ")(OUTSIDE). Remove it." << endl;
    target_date.assign(unlimited);
    return;
  }
  initial.assign(target_date);
  // To convert "YYYYmmdd_HHMMSS" to "YYYY-mm-dd HH:MM:SS"
  target_date.assign("'");
  target_date.append(initial.substr(0, 4));
  target_date.append("-");
  target_date.append(initial.substr(4, 2));
  target_date.append("-");
  target_date.append(initial.substr(6, 2));
  target_date.append(" ");
  target_date.append(initial.substr(9, 2));
  target_date.append(":");
  target_date.append(initial.substr(11, 2));
  target_date.append(":");
  target_date.append(initial.substr(13, 2));
  target_date.append("'");

  title[4] = '_';
}

// Get current time (format: YYYYmmdd_HHMMSS)
string Conditions::GetTimeNow() const
{
  time_t rawtime;
  time(&rawtime);
  struct tm * timeinfo = localtime(&rawtime);
  char str[20] = {'\0'};
  strftime(str, 16, "%Y%m%d_%H%M%S", timeinfo);
  return str;
}

// Testing if this instance represents a valid condition
bool Conditions::ValidCondition() const
{
  if(title.empty())
  {
    cerr << "Empty column..." << endl;
    return false;
  }

  if(low_edge.empty() || high_edge.empty())
  {
    cerr << "Empty edge(s) (|" << low_edge << "| - |" << high_edge << "|)..." << endl;
    return false;
  }

  if(unlimited == low_edge || unlimited == high_edge)
    return false;

  return true;
}

// Convert this condition using the format of MySQL query command
string Conditions::Output(Conditions::AndState concatenation) const
{
  if(!state_of_this_class)
  {
    cerr << "Conditions not ready..." << endl;
    return "";
  }

  string results;
  // To add leading " and " to the command
  if(concatenation & 1)
    results.append(" and ");
  results.append(title);
  if(low_edge == high_edge)
  {
    results.append(" = ");
    results.append(low_edge);
  }
  else
  {
    results.append(" between ");
    results.append(low_edge);
    results.append(" and ");
    results.append(high_edge);
  }
  // To add tailing " and " to the command
  if(concatenation & 2)
    results.append(" and ");
  return results;
}

// To convert a number into a string
void Conditions::Convert(double v, string& s)
{
  ostringstream os;
  os << v;
  s.assign(os.str());
}

// To turn string into 'string' (add quotes)
inline void Conditions::AddQuote(const string& source, string& s)
{
  s.assign("'");
  s.append(source);
  s.append("'");
}

// Factory method. Generate an instance of Conditions
// Need 1 argument:
//   1. hint of a condition
// Returns the Conditions instance if created, or throw an exception
Conditions Conditions::Factory(const string& hint) throw(BadGeneration)
{
  if(hint.empty())
    throw BadGeneration("Empty string.");
  if("UNDEFINED" == hint)
    return Conditions();

  if(string::npos == hint.find("="))
    throw BadGeneration("No range.");

  // Find "="
  size_t equal_mark = hint.find("=");
  string title(hint.substr(0, equal_mark));
  // To remove leading hyphan(s) (1 or 2) of <title>.
  if('-' == title[0])
  {
    if('-' == title[1])
      title.erase(0, 2);
    else
      title.erase(0, 1);
  }

  if(!FactoryInitializer::GetInstance().ValidTitle(title))
    throw BadGeneration("Invalid title: " + title);

  string low(hint.substr(equal_mark+1));
  string high = low;
  if(string::npos != low.find("-"))
  {
    if(low.size() == count_if(low.begin(), low.end(), bind1st(equal_to<char>(), '-')))
    {
      cout << "No limitation for " << title << " received." << endl;
      return Conditions();
    }

    size_t hyphen = count(low.begin(), low.end(), '-');
    if(1 == hyphen)	// Only Low-High
      hyphen = low.find("-");
    else if('-' == *low.begin())	// <-High>
      hyphen = 0;
    else if('-' == *low.rbegin())	// <Low->
      hyphen = low.size();
    else if(hyphen%2)	// Hyphen in Low & High: the same format required
    {
      boost::iterator_range<string::iterator> it = boost::find_nth(low, "-", (hyphen-1)/2);
      hyphen = it.begin() - low.begin();
    }
    else
      throw BadGeneration("Invalid hyphan: " + low);

    if(0 == hyphen)	// <-High>
    {
      high.erase(0, 1);
      return Conditions(title, "UNLIMITED", high);
    }
    else if(low.size() == hyphen)	// <Low->
    {
      low.erase(hyphen-1);
      return Conditions(title, low, "UNLIMITED");
    }
    else	// <Low-High>
    {
      low.erase(hyphen);
      high.erase(0, hyphen+1);
    }
  }

  if(low < high)
    return Conditions(title, low, high);
  else
    return Conditions(title, high, low);
}

// Handles the comment (a column in the table of database)
void Conditions::CommentConditions(int comment_status, string& command)
{
  if(UnlimitedComments == comment_status)
    return;
  if(0 > comment_status || 2 < comment_status)
  {
    cout << "Warning... Illegal status of comment received: " << comment_status << endl;
    return;
  }


  if(WithoutCommentsOnly == comment_status)
  {
    vector<string> conditions(3);
    conditions[0].assign("error_code = '0'");
    conditions[1].assign("comment in ('NULL', 'NONE', '')");
    conditions[2].assign("good = '1'");
    command.append(boost::join(conditions, " and "));
  }
  else
  {
    vector<string> conditions(3);
    conditions[0].assign("error_code <> '0'");
    conditions[1].assign("comment not in ('NULL', 'NONE', '')");
    conditions[2].assign("good = '0'");
    command.append(" (");
    command.append(boost::join(conditions, " or "));
    command.append(") ");
  }
}

// Definition of static const
const string Conditions::unlimited = "UNLIMITED";
map<string, Conditions::string_processer> Conditions::function_pool;


// ===> ArgumentParser <===
// Arguments (if any) parser for SelectionTool
// Constructor
ArgumentParser::ArgumentParser():
  print_level(-1),
  comment_status(Conditions::UnlimitedComments)
{}

// This is where terminal arguments are parsed.
// Need 2 arguments:
//   1. argc (of main)
//   2. argv (of main)
// Returns 0 if success, non-0 otherwise
int ArgumentParser::Parse(int argc, const char* argv[])
{
  if(1 == argc)
  {
    cerr << "Need at least an argument to run this program..." << endl;
    return 1;
  }

  // Create description of all allowed options
  boost::program_options::options_description opts("SelectionTool: display files that apply");
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
    ("not-good,N", "Enable the use of non-good records")
    ("output,O", value<string>(), "Save the output into this file")
    ("redirect,R", value<string>(), "Only save the output into this file")
    ("filename,f", value<vector<string> >()->multitoken()->composing(), "Name (hints) of root file (\"*\" accepted)")
    ("print-level", value<int>()->default_value(-1), "Items to print (the lower the more)")
    ("comment-state,C", value<int>()->default_value(Conditions::WithoutCommentsOnly), "0: no comment; 1: with comment; 2: not-limited")
    ("comment-required", "Only print records with comments (comment-state=1)")
    ("comment-uncontrolled", "Print records with and without comments (comment-state=2)")
  ;

  // Reserve repulsive options
  ReserveRepulsiveOptions("comment-uncontrolled", "comment-required");
  ReserveRepulsiveOptions("output", "redirect");

  // Still adding items
  for(set<string>::const_iterator it = FactoryInitializer::GetInstance().option_begin(); it != FactoryInitializer::GetInstance().option_end(); ++it)
  {
    size_t semicolon = it->find(";");
    adding.operator()(it->substr(0, semicolon).c_str(), value<string>(), it->substr(semicolon+1).c_str());
  }

  // Parse the arguments
  boost::program_options::variables_map vm;
  try {
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
  } catch(exception& e) {
    cerr << "Error parsing arguments..." << endl;
    cerr << e.what() << endl;
    return 1;
  }

  if(vm.count("help"))
  {
    // If there is a "help", help, and exit (-10 affects how the main function behaves)
    cout << opts << endl;
    cout << "(For detailed help, try \"-m\".)" << endl;
    return -10;
  }
  else if(vm.count("manual"))
  {
    // Print detailed help messages
    ifstream in("SelectionTool.help");
    if(!in.is_open())
      in.open("../SelectionTool.help");
    if(in.is_open())
    {
      cout << in.rdbuf() << endl;
      in.close();
      return -10;
    }
    else
    {
      cerr << "Error! Can't find help messages for SelectionTool..." << endl;
      return 2;
    }
  }

  // Initiate the map, pairs of key(item)-value(content modifier)
  Conditions::MapInitiater::InitiateMap();

  // Find out comment-state
  try {
    comment_status = vm["comment-state"].as<int>();
  } catch(exception& e) {
    cerr << "Illegal arguments: " << vm["comment-state"].as<string>() << endl;
    cerr << e.what() << endl;
    return 2;
  }
  // Testing if invalid comment status.
  if(0 > comment_status || 2 < comment_status)
  {
    cerr << "Error! Status of comment shall be 0, 1, or 2 while " << comment_status << " received." << endl;
    return 2;
  }
  // If comment_status remains 0, no "comment-state" is given. Try two long
  // options, then.
  if(0 == comment_status)
  {
    if(vm.count("comment-required"))
      comment_status = 2;
    else if(vm.count("comment-uncontrolled"))
      comment_status = 1;
  }
  else if(2 == comment_status && vm.count("comment-required"))
  {
    // Contradiction detected.
    cerr << "Error! \"comment-state\" wants not comments with \"comment-required\" provided." << endl;
    return 2;
  }
  else if(1 == comment_status && vm.count("comment-uncontrolled"))
  {
    // Contradiction detected.
    cerr << "Error! \"comment-state\" insists comments with \"comment-uncontrolled\" provided." << endl;
    return 2;
  }

  // Get general parameters used by both SelectionTool (also used by
  // OperationTool)
  try {
    GetGeneralParameters(vm, print_level = vm["print-level"].as<int>());
  } catch(runtime_error& e) {
    cerr << "Parsing terminal arguments failed..." << endl;
    cerr << e.what() << endl;
    return 2;
  } catch(exception& e) {
    cerr << "Illegal arguments: " << vm["print-level"].as<string>() << endl;
    cerr << e.what() << endl;
    return 2;
  }

  // For print level (only columns with level above print_level will be
  // displayed)
  if(2 < print_level)
  {
    cerr << "Printing level is so high (" << print_level <<
            ") that no columns will be displayed..." << endl;
    return 2;
  }
  else if(-1 > print_level)
  {
    cout << "Warning... Printing level is unreasonably low (" << print_level <<
            ")... Use default |-1|." << endl;
    print_level = -1;
  }

  // Take in each condition
  for(set<string>::const_iterator it = FactoryInitializer::GetInstance().title_begin(); it != FactoryInitializer::GetInstance().title_end(); ++it)
  {
    if(!vm.count(*it))
      continue;

    string info(*it);
    info.append("=");
    info.append(vm[*it].as<string>());
    conditions.push_back(Conditions::Factory(info));
  }

  // Remove invalid conditions if any
  vector<Conditions>::iterator last = remove_if(conditions.begin(), conditions.end(), not1(mem_fun_ref(&Conditions::ValidCondition)));
  if(conditions.end() != last)
    conditions.erase(last, conditions.end());

  // Set output method
  if(vm.count("output"))
    SetOutputFile(vm["output"].as<string>(), 0);
  else if(vm.count("redirect"))
    SetOutputFile(vm["redirect"].as<string>(), 1);

  // For conditions concerning name of ROOT files...
  if(vm.count("filename"))
  {
    vector<string> filename = vm["filename"].as<vector<string> >();
    if(filename.empty())
      cout << "Warning... Trivial file name received..." << endl;
    else
    {
      for(vector<string>::iterator it = filename.begin(); it != filename.end(); ++it)
      {
        // If there is a star, replace it with "%", MySQL version for "*".
        if(string::npos == (*it).find("*"))
          (*it).insert(0, " lfn = '");
        else
        {
          boost::replace_all(*it, "*", "%");
          (*it).insert(0, " lfn like '");
        }
        (*it).append("'");
      }
      // Store the file hint(s)
      if(1 == filename.size())
        file_hints.assign(filename[0]);
      else
      {
        file_hints.assign("(");
        file_hints.append(boost::join(filename, " or "));
        file_hints.append(")");
      }
    }
  }

  return 0;
}

// Generate the condition part of MySQL (selection) query command (after "WHERE")
// There is a "WHERE" in the string returned.
string ArgumentParser::MySQLCommand() const
{
  // Number of conditions other than comments and file names
  size_t N = N_conditions();
  if(0 == N &&
     Conditions::UnlimitedComments == comment_status &&
     file_hints.empty())
  {
    // No conditions at all.
    cout << "Empty condition..." << endl;
    return "";
  }

  string common_command;
  Conditions::CommentConditions(comment_status, common_command);
  if(0 == N)
  {
    // If there is no condition for the fields other than comments and file
    // names, add the conditions for the latter two if any, and returns.
    if(!common_command.empty())
      common_command.insert(0, " where ");
    AddFileHint(common_command);
    return common_command;
  }

  string command(" where ");
  // To construct the concatenation of all other conditions
  if(1 == N)
    command.append(conditions.front().Output(Conditions::Neither));
  else
  {
    --N;
    for(size_t i=0;i<N;++i)
      command.append(conditions[i].Output(Conditions::OnlyTail));
    command.append(conditions[N].Output(Conditions::Neither));
  }

  // To add the comment condition to the final comment.
  if(!common_command.empty())
  {
    command.append(" and ");
    command.append(common_command);
  }

  // Add the file name condition.
  AddFileHint(command);

  // Return the concatenation of all conditions
  return command;
}

// Output the name of columns according to the level
// Need 2 argument:
//   1. outer container
//   2. minimal level (default: -2)
void ArgumentParser::Columns(vector<string>& target, int level) const
{
  if(!target.empty())
    target.clear();

  size_t scale = name_of_columns.size();
  if(0 == scale)
    return;

  if(-2 == level)
    level = print_level;
  else if(3 < level || -2 > level)
    return;

  target.reserve(scale);
  for(size_t i=0;i<scale;++i)
  {
    if(level < level_of_columns[i])
      target.push_back(name_of_columns[i]);
  }
}

// Output all names of columns as a string (all names are joined by ", ")
string ArgumentParser::ColumnNames(int level) const
{
  vector<string> target;
  Columns(target, level);
  return boost::join(target, ", ");
}

// Set output file
bool ArgumentParser::SetOutputFile(const string& filename, bool surpress)
{
  if(filename.empty())
  {
    cerr << "Empty name of output..." << endl;
    return false;
  }
  output_file.assign(filename);
  ifstream in(output_file.c_str());	// Testing if that file exists
  if(in.is_open())
  {
    cout << "Warning... File " << output_file << " exists... Overwriting it..." << endl;
    in.close();
  }

  ofstream out(output_file.c_str());	// Try opening it in write mode
  if(!out.is_open())
  {
    cerr << "Can't access " << output_file << " to write..." << endl;
    return false;
  }
  else
  {
    out.close();
    surpressing = surpress;
    return true;
  }
}

// Add conditions concerning about filenames
void ArgumentParser::AddFileHint(string& command) const
{
  if(file_hints.empty())
    return;

  if(command.empty())
  {
    // In case of empty command
    command.assign(" where ");
    command.append(file_hints);
  }
  else if(string::npos == command.find(" where ") &&
          string::npos == command.find("where "))
  {
    // <command> is a command, but no "where" inside (no conditions added)
    string the_beginning(" where ");
    the_beginning.append(file_hints);
    the_beginning.append(" and ");
    command.append(the_beginning);
  }
  else
  {
    // <command> is a command with conditions (" where <condition 1>, ...")
    string the_beginning("where ");
    the_beginning.append(file_hints);
    the_beginning.append(" and ");
    // Use file hints as the first condition for efficiency concerns.
    boost::replace_first(command, "where ", the_beginning);
  }
}


// ===> FactoryInitializer <===
// Manager of title of Conditions. Only titles accepted by FactoryInitializer
// could be used to name a Conditions
// Constructor
FactoryInitializer::FactoryInitializer():
  allowed_titles()
{
  InsertTitle("SvnRev,r;Revision of DMPSW by which ROOT files are generated");
  InsertTitle("version,v;Version of DMPSW");
  InsertTitle("energy,e;Energy range (MeV)");
  InsertTitle("last-modified,l;Time of last modified (YYYYMMDD-YYYYMMDD)");
  InsertTitle("timestamp,t;Time of last modified range");
  InsertTitle("Size,s;Size range of a ROOT file (MB)");
  InsertTitle("Entries,n;Number of event per ROOT file");
}

// Singletonship
FactoryInitializer& FactoryInitializer::GetInstance()
{
  static FactoryInitializer instance;
  return instance;
}

// Insert a title
void FactoryInitializer::InsertTitle(const string& title)
{
  if(title.empty())
    return;

  size_t semicolon = title.find(";");
  if(string::npos == semicolon)
  {
    cout << "Warning... Invalid format of title |" << title << "|: no semicolon..." << endl;
    return;
  }

  string name = title.substr(0, semicolon);
  // allowed_titles should be simply the long option.
  if(string::npos == name.find(","))
    allowed_titles.insert(name);
  else
    allowed_titles.insert(name.substr(0, name.find(",")));

  string new_title(title);
  // (R) in description of an option means this option, for a condition, could
  // be range.
  new_title.append(" (R)");
  title_options.insert(new_title);
}


// ===> SelectionTool <===
// Main tool for fetching records from database
// Constructor
SelectionTool::SelectionTool():
  MySQLInterface(),
  holder()
{}

// Initiate the database connection, initiate the RecordsHolder, find the MySQL
// (selection) query command
bool SelectionTool::InitiateDatabase(const ArgumentParser& parser)
{
  if(!connectMySQL(parser.Server().c_str(), parser.User().c_str(), parser.Passwd().c_str(), (database = parser.Database()).c_str(), parser.Port()))
  {
    cerr << "Connection failed!" << endl;
    return false;
  }

  if(!holder.Initialize(parser))
  {
    cerr << "Error initiating buffer..." << endl;
    return false;
  }

  // Use print level to select the columns wanted.
  int level = parser.PrintLevel();
  if(-1 == level)
    command.assign("select * from ");
  else
  {
    command.assign("select ");
    command.append(parser.ColumnNames());
    command.append(" from ");
  }

  // Append table to the command
  command.append(parser.Table());

  // Append conditions.
  string conditions(parser.MySQLCommand());
  if(conditions.empty())
    cerr << "Warning... No conditions detected: printing all" << endl;
  else
    command.append(conditions);

  output.assign(parser.Output());
  surpressing = parser.Surpressing();

  return true;
}

// Main method. Combines selecting and displaying.
void SelectionTool::SelectAndDisplay()
{
  // Asking for records.
  size_t N_records = FetchingRecords();
  if(0 == N_records)
  {
    cout << "No records detected..." << endl;
    return;
  }

  // Display the record(s).
  if(output.empty())
    Print();
  else
  {
    if(!surpressing)
      Print();
    ofstream out(output.c_str());
    if(!out.is_open())
    {
      cerr << "Can't open a \"" << output << "\" for redirecting output..." << endl;
      return;
    }

    Print(out);
  }
}

// In this method MySQL query is queried.
// Returns the size of records (not the number of columns of each record)
size_t SelectionTool::FetchingRecords()
{
  // "command" has been verified
  vector<vector<string> > data;
//cout << "command: |" << command << "|." << endl;
  if(!GetDataFromDB(command, data))
  {
    cout << "Error getting records..." << endl;
    return 0;
  }

  // Number of records
  size_t NNN = data.size();
  for(size_t i=0;i<NNN;++i)
  {
    vector<string>& one_set = data[i];
    // Transform the record by replacing NULL with a string "NULL".
    for_each(one_set.begin(), one_set.end(), Replacing());
    // Insert the transformed record.
    holder.Insert(one_set);
  }

  return NNN;
}

