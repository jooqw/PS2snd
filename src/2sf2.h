#ifndef S2SF2_H
#define S2SF2_H

#include "main.h"
#include "hd.h"
#include "bd.h"
#include <QString>

class Sf2Exporter {
public:
    static bool exportToSf2(const QString& path, const Bank& bank, BDParser* bd);
};

#endif // S2SF2_H
