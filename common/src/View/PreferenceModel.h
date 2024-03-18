#pragma once

#include <QAbstractTableModel>

#include "Color.h"
#include "Preference.h"

namespace TrenchBroom::View {

typedef std::tuple<QString, QString, QString> StringTuple3;


class PreferenceModel : public QAbstractTableModel {
Q_OBJECT
public:
  enum Columns {
    Index, Context, Path, Value, Default, Type, count,
  };

  QString columnNames[Columns::count]{"Index", "Context", "Path", "Value", "Default", "Type"};

private:
  std::vector<PreferenceBase *> m_preferences;
  //  int m_colorsCount;

public:
  explicit PreferenceModel(QObject *parent = nullptr);

  void reset();

  int rowCount(const QModelIndex &parent) const override;

  int columnCount(const QModelIndex &parent) const override;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  QVariant data(const QModelIndex &index, int role) const override;

  bool setData(const QModelIndex &index, const QVariant &value, int role) override;

  Qt::ItemFlags flags(const QModelIndex &index) const override;

  PreferenceBase *dataAt(const size_t &index);

  size_t preferenceCount() const;
  // void pickColor(const QModelIndex &index);

private:
  void initialize();

  std::vector<PreferenceBase *> data() const;

  StringTuple3 getInfo(PreferenceBase *preferenceBase) const;


  // Preference<Color> *getColorPreference(int index) const;

  // bool checkIndex(const QModelIndex &index) const;
};
} // namespace TrenchBroom::View
