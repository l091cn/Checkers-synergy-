#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }

    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }
    // Функция reload() перезагружает настройки из файла settings.json
    // Она открывает файл, считывает JSON-содержимое в объект config и закрывает файл
    // Это позволяет обновить настройки во время работы программы без необходимости перезапуска

    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
