#ifndef PLAYDATE_H
#define PLAYDATE_H

#include <QDateTime>

class PlayDate
{
public:
    PlayDate();
    void init(QDate, QDate, QTime, QTime);
    bool isValidDate(QDateTime);
public:
    QDate startDate;
    QDate endDate;
    QTime startTime;
    QTime endTime;
};

#endif // PLAYDATE_H
