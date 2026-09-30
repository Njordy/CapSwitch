# Публикация CapSwitch 1.2.1

Исходники отправляются через GitHub Desktop. Готовые EXE и ZIP прикрепляются к релизу на сайте GitHub отдельным шагом.

## 1. Отправить исходники через Desktop

1. В GitHub Desktop выберите **File → Add local repository**.
2. Укажите папку `D:\UE_Plugins\CapSwitch\CapSwitch` — именно её, а не внешнюю папку с соседним Switchy-master.
3. Просмотрите Changes. Здесь должны быть исходники, иконка, assets, scripts, tests и документация. Папки artifacts, x64, настройки .claude и промежуточные файлы сборки исключены.
4. В Summary напишите `Release CapSwitch 1.2.1` и нажмите **Commit to…** для выбранной ветки.
5. Если репозиторий ещё не опубликован, нажмите **Publish repository**. Имя — `CapSwitch`; для публичного проекта снимите **Keep this code private**. Если он уже опубликован, нажмите **Push origin**.

## 2. Создать релиз на сайте

1. Откройте страницу опубликованного репозитория. Справа выберите **Releases → Draft a new release** (при первом релизе подпись может отличаться).
2. В выборе тега создайте **v1.2.1**. Target должен указывать на ветку с только что отправленной версией.
3. Название: **CapSwitch 1.2.1**.
4. Скопируйте описание из `docs\RELEASE-NOTES.md`.
5. Прикрепите из `artifacts\release-1.2.1`:
   - `CapSwitch-1.2.1-windows-x64.exe`;
   - `CapSwitch-1.2.1-windows-x64.zip`;
   - `SHA256SUMS.txt`.
6. Можно сначала выбрать **Save draft**, проверить вложения, затем **Publish release**. Для обычного выпуска отметьте **Set as latest release**; prerelease нужен только если вы намеренно публикуете тестовую версию.

Скачивающим достаточно EXE либо ZIP. В ZIP уже есть программа и краткая инструкция. Автоматические ссылки GitHub **Source code (zip/tar.gz)** содержат исходники, а не собранный EXE. PDB, OBJ и тестовые программы прикреплять не нужно.

Ничего не опубликовано автоматически: эти действия выполняете вы.

## 3. Следующий выпуск

Обновите номер в `CapSwitch\version.h` (числовые компоненты, tuple и строки), README, CHANGELOG, RELEASE-NOTES и PORTABLE-README. Затем из папки проекта выполните:

```powershell
.\scripts\Build-Release.ps1
```

Скрипт найдёт Visual Studio, выполнит тесты, соберёт EXE и подготовит файлы с новым номером. Он не заменяет работающий экземпляр. После изменения готового EXE заново запускайте упаковку, чтобы ZIP и контрольные суммы совпадали.

## Источники

- [Публикация проекта через GitHub Desktop](https://docs.github.com/en/desktop/adding-and-cloning-repositories/adding-an-existing-project-to-github-using-github-desktop).
- [Создание релиза и добавление двоичных файлов](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).


