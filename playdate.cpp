#include "playdate.h"

PlayDate::PlayDate()
{

}

void PlayDate::init(QDate sDate, QDate eDate, QTime sTime, QTime eTime)
{
    startDate = sDate;
    endDate = eDate;
    startTime = sTime;
    endTime = eTime;
}

bool PlayDate::isValidDate(QDateTime currentDateTime)
{
    QDate cDate = currentDateTime.date();
    QTime cTime = currentDateTime.time();
    if(cDate >= startDate && cDate <= endDate && cTime >= startTime && cTime <= endTime)
        return true;
    return false;
}

