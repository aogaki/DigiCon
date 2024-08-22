#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

void PrintJSONKeys(const nlohmann::json &element, const std::string key)
{
  std::string value = "No Value";
  std::string accessMode = "No Value";
  std::string description = "No Value";
  try {
    value = element.at("value");
  } catch (...) {
  }
  try {
    accessMode = element.at("accessmode").at("value");
  } catch (...) {
  }
  try {
    description = element.at("description").at("value");
  } catch (...) {
  }

  if (accessMode == "READ_WRITE")
    std::cout << key << "\t" << accessMode << "\t" << value << "\t"
              << description << std::endl;
}

void PrintClassSkeletonVariables(std::ofstream &output,
                                 const nlohmann::json &element,
                                 const std::string key)
{
  std::string value = "No Value";
  std::string accessMode = "No Value";
  std::string description = "No Value";
  try {
    value = element.at("value");
  } catch (...) {
  }
  try {
    accessMode = element.at("accessmode").at("value");
  } catch (...) {
  }
  try {
    description = element.at("description").at("value");
  } catch (...) {
  }

  if (accessMode == "READ_WRITE") {
    if (key.find("ch_") != std::string::npos)
      output << "  std::vector<std::string> " << key
             << " = std::vector<std::string>(nChs, \"" << value << "\"); // "
             << description << std::endl;
    else
      output << "  std::string " << key << " = \"" << value << "\"; // "
             << description << std::endl;
  }
}

void PrintClassSkeletonGetterSetter(std::ofstream &output,
                                    const nlohmann::json &element,
                                    const std::string key)
{
  std::string value = "No Value";
  std::string accessMode = "No Value";
  std::string description = "No Value";
  try {
    value = element.at("value");
  } catch (...) {
  }
  try {
    accessMode = element.at("accessmode").at("value");
  } catch (...) {
  }
  try {
    description = element.at("description").at("value");
  } catch (...) {
  }

  if (accessMode == "READ_WRITE") {
    if (key.find("ch_") != std::string::npos) {
      output << "  std::string Get_" << key << "(uint32_t ch) { return " << key
             << "[ch]; }" << std::endl;
      output << "  void Set_" << key
             << "(const std::string &value, const uint32_t &ch);" << std::endl;
    } else {
      output << "  const std::string Get_" << key << "() { return " << key
             << "; }" << std::endl;
      output << "  void Set_" << key << "(const std::string &value);"
             << std::endl;
    }
    output << std::endl;
  }
}

void PrintClassSetter(std::ofstream &output, const nlohmann::json &element,
                      const std::string key, const std::string className)
{
  std::string value = "No Value";
  std::string accessMode = "No Value";
  std::string description = "No Value";
  std::string datatype = "No Value";
  std::vector<std::string> allowedvalues;
  try {
    value = element.at("value");
  } catch (...) {
  }
  try {
    accessMode = element.at("accessmode").at("value");
  } catch (...) {
  }
  try {
    description = element.at("description").at("value");
  } catch (...) {
  }
  try {
    datatype = element.at("datatype").at("value");
    if (datatype == "NUMBER") {
      allowedvalues.push_back(element.at("minvalue").at("value"));
      allowedvalues.push_back(element.at("maxvalue").at("value"));
      allowedvalues.push_back(element.at("increment").at("value"));
    } else if (datatype == "STRING") {
      auto nValues = std::stoi(
          static_cast<std::string>(element.at("allowedvalues").at("value")));
      for (auto i = 0; i < nValues; i++) {
        allowedvalues.push_back(
            element.at("allowedvalues").at(std::to_string(i)).at("value"));
      }
    } else {
      std::cout << "WTF? " << datatype << std::endl;
    }
  } catch (...) {
  }

  if (accessMode == "READ_WRITE") {
    output << "  void " << className << "::Set_" << key;

    if (key.find("ch_") != std::string::npos) {
      output << "(const std::string &value, const uint32_t &ch) {" << std::endl;
      output << "std::cout << \"Setting " << key << " ch\" << ch << std::endl;"
             << std::endl;
    } else {
      output << "(const std::string &value) {" << std::endl;
      output << "std::cout << \"Setting " << key << "\" << std::endl;"
             << std::endl;
    }

    output << "std::string defaultVal = \"" << value << "\";" << std::endl;

    for (auto i = 0; i < allowedvalues.size(); i++) {
      if (i == 0)
        output << "std::vector<std::string> allowedValue = {\""
               << allowedvalues[i] << "\"";
      else
        output << ", \"" << allowedvalues[i] << "\"";
    }
    output << "};" << std::endl;

    output << "auto val = CheckValue(value, allowedValue, defaultVal);"
           << std::endl;

    if (key.find("ch_") != std::string::npos) {
      output << key << "[ch] = val; // " << description << std::endl;
    } else {
      output << key << " = val; // " << description << std::endl;
    }

    output << "}" << std::endl;
    output << std::endl;
  }
}

void read_device_tree()
{
  auto FW = std::string("PHA");
  auto fileName = FW + std::string("_tree.json");
  auto inputFile = std::ifstream(fileName);
  auto treeJSON = nlohmann::json::parse(inputFile);
  auto className = std::string("Parameter") + FW;

  // std::cout << "Top level keys:" << std::endl;
  // for (auto &ele : treeJSON.items()) {
  //   std::cout << ele.key() << std::endl;
  // }

  std::ofstream headerFile(className + ".hpp");

  headerFile << "#include <iostream>\n#include <nlohmann/json.hpp>\n"
                "#include <string>\n#include <vector>\n"
             << std::endl;

  headerFile << "class " << className << "\n{\n private:" << std::endl;
  headerFile << "\n// Mod keys:" << std::endl;
  for (auto &ele : treeJSON.at("par").items()) {
    PrintClassSkeletonVariables(headerFile, ele.value(), ele.key());
  }

  headerFile << "\n// Ch keys:" << std::endl;
  headerFile << "const uint32_t nChs = 16;" << std::endl;
  for (auto &ele : treeJSON.at("ch").at("0").at("par").items()) {
    PrintClassSkeletonVariables(headerFile, ele.value(), ele.key());
  }

  headerFile << "\nstd::string CheckValue(const std::string &value, const "
                "std::vector<std::string> &allowedValue, const std::string "
                "&defaultVal);"
             << std::endl;
  headerFile << "std::string CheckValueInt(const std::string &value, const "
                "std::vector<std::string> &allowedValue, const std::string "
                "&defaultVal);"
             << std::endl;
  headerFile << "std::string CheckValueDouble(const std::string &value, const "
                "std::vector<std::string> &allowedValue, const std::string "
                "&defaultVal);"
             << std::endl;

  headerFile << "public:" << std::endl;

  headerFile << "\n// Mod keys:" << std::endl;
  for (auto &ele : treeJSON.at("par").items()) {
    PrintClassSkeletonGetterSetter(headerFile, ele.value(), ele.key());
  }

  headerFile << "\n// Ch keys:" << std::endl;
  for (auto &ele : treeJSON.at("ch").at("0").at("par").items()) {
    PrintClassSkeletonGetterSetter(headerFile, ele.value(), ele.key());
  }
  headerFile << "};\n";
  headerFile.close();

  std::ofstream sourceFile(className + ".cpp");
  sourceFile << "#include \"" << className << ".hpp\"\n\n";

// cout following code
  // std::string ParameterPHA::CheckValue(
  //     const std::string &value, const std::vector<std::string> &allowedValue,
  //     const std::string &initialVal)
  // {
  //   if (std::find(allowedValue.begin(), allowedValue.end(), value) ==
  //       allowedValue.end()) {
  //     std::cerr << "Value " << value << " not allowed. Using default value "
  //               << initialVal << std::endl;
  //     return initialVal;
  //   }
  //   return value;
  // }
  sourceFile << "std::string " << className << "::CheckValue(const std::string &value, const std::vector<std::string> &allowedValue, const std::string &initialVal)\n{\n";
  sourceFile << "  if (std::find(allowedValue.begin(), allowedValue.end(), value) == allowedValue.end()) {\n";
  sourceFile << "    std::cerr << \"Value \" << value << \" not allowed. Using default value \" << initialVal << std::endl;\n";
  sourceFile << "    return initialVal;\n";
  sourceFile << "  }\n";
  sourceFile << "  return value;\n}\n\n";


  sourceFile << "\n// Mod keys:" << std::endl;
  for (auto &ele : treeJSON.at("par").items()) {
    PrintClassSetter(sourceFile, ele.value(), ele.key(), className);
  }

  sourceFile << "\n// Ch keys:" << std::endl;
  for (auto &ele : treeJSON.at("ch").at("0").at("par").items()) {
    PrintClassSetter(sourceFile, ele.value(), ele.key(), className);
  }
  sourceFile.close();
}
