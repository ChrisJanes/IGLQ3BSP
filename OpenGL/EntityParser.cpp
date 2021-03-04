#include "EntityParser.h"

#include <sstream>

float entity::get_float(std::string key)
{
    auto pair = pairs.find(key);
    if(pair == pairs.end())
        return 0.0f;

    return std::atof(pair->second.c_str());
}

std::string entity::get_string(std::string key)
{
    auto pair = pairs.find(key);
    if (pair == pairs.end())
        return "";

    return pair->second;
}

void entity::get_vec3(std::string key, glm::vec3& vec)
{
    auto pair = pairs.find(key);
    if (pair == pairs.end())
        return;

    std::string val = pair->second;

    double v1, v2, v3;
    v1 = v2 = v3 = 0;
    sscanf_s(val.c_str(), "%lf %lf %lf", &v1, &v2, &v3);
    vec.x = v1;
    vec.y = v2;
    vec.z = v3;

}

entity EntityParser::parse(std::string ent)
{
    entity new_ent;

    std::string line;
    std::stringstream entdata;
    entdata << ent;

    entdata >> line;

    if (line != "{")
        return new_ent;

    do
    {
        std::string key;
        std::string value;

        std::getline(entdata, line);
        if (line == "}") break;
        if (line.size() == 0) continue;

        int split = line.find(' ');
        key = line.substr(0, split);
        value = line.substr(split + 1, line.size() - 1);

        key = key.substr(1, key.size() - 2);
        value = value.substr(1, value.size() - 2);
        new_ent.pairs.insert(std::pair<std::string, std::string>(key, value));

    } while (true);

    return new_ent;
}

void EntityParser::parse(std::string entities, std::vector<entity>& store)
{
    // break the entities string into lumps and parse each lump into an entity.
    do
    {
        std::string ent;
        int index = entities.find("}");
        if (index == -1)
            break;
        ent = entities.substr(0, index+1);
        entities.erase(0, index+2);

        store.push_back(parse(ent));

    } while (true);
}
