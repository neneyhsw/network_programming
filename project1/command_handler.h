#include <bits/stdc++.h>
using namespace std;

void RemoveVectorSpaces(vector<string> &v, string delimiter) {
  vector<string>::iterator iter;
  for (iter = v.begin(); iter != v.end();) {
    if (*iter == delimiter)
      v.erase(iter);
    else
      iter++;
  }
}

// for string delimiter
vector<string> SplitString (string s, string delimiter) {
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  string token;
  vector<string> res;

  while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
    token = s.substr (pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back (token);
    res.push_back(delimiter);  // yhsw <++
  }

  res.push_back (s.substr (pos_start));
  return res;
}

vector<string> GetCmd(string input) {
  if (input == "") {
    return vector<string> ();
  }

  vector<string> temp_split_cmd, space_split_cmd, split_cmd;
  temp_split_cmd = SplitString(input, "|");
  for (int i = 0; i < temp_split_cmd.size(); i++) {
    space_split_cmd = SplitString(temp_split_cmd[i], " ");
    for (int j = 0; j < space_split_cmd.size(); j++)
      split_cmd.push_back(space_split_cmd[j]);
  }
  RemoveVectorSpaces(split_cmd, "");  // remove multiple spaces between two parameters
  RemoveVectorSpaces(split_cmd, " ");  // remove space in cmd that generate by first split "|"

  // check if the last space of split_cmd is "!"
  vector<string> temp_exclamation;
  if (split_cmd[split_cmd.size()-1].find("!") != string::npos) {
    temp_exclamation = SplitString(split_cmd[split_cmd.size()-1], "!");
    RemoveVectorSpaces(temp_exclamation, "");  // remove redundant array space
    split_cmd.erase(split_cmd.end());  // remove the last array space (include "!")
    split_cmd.push_back(temp_exclamation[0]);
    split_cmd.push_back(temp_exclamation[1]);
  }

  return split_cmd;
}
