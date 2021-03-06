#include "selection_tool.h"
#include <iostream>
using namespace std;

int main(int argc, const char* argv[])
{
  if(1 == argc)
  {
    cerr << "Need arguments to run this program..." << endl;
    return 1;
  }

  // First, parse the terminal arguments if any
  ArgumentParser parser;
  int parse_state = parser.Parse(argc, argv);
  if(-10 == parse_state)
    return 0;
  else if(parse_state)
  {
    cerr << "Can't parse the arguments..." << endl;
    return parse_state;
  }

  // Create a SelectionTool
  SelectionTool tool;

  // Initiate the database...
  if(!tool.InitiateDatabase(parser))
    return 1;

  // Select records, and display
  tool.SelectAndDisplay();

  return 0;
}
