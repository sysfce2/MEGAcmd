/**
 * @file src/megacmdshellcommunicationsnamedpipes.h
 * @brief MEGAcmd: Communications module to connect to server using NamedPipes
 *
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGAcmd.
 *
 * MEGAcmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 *
 * This file is also distributed under the terms of the GNU General
 * Public License, see http://www.gnu.org/copyleft/gpl.txt for details.
 */

#ifndef MEGACMDSHELLCOMMUNICATIONSNAMEDPIPES_H
#define MEGACMDSHELLCOMMUNICATIONSNAMEDPIPES_H
#ifdef _WIN32

#include "megacmdshellcommunications.h"

#include <windows.h>
#include <Lmcons.h> //getusername

#define ERRNO WSAGetLastError()
#include <string>
#include <iostream>
#include <thread>
#include <mutex>

#include <Shlwapi.h> //PathAppend

namespace megacmd {
typedef struct structListenStateChangesNamedPipe{
    int receiveNamedPipeNum;
    void (*statechangehandle)(std::string);
} sListenStateChangesNamedPipe;


class MegaCmdShellCommunicationsNamedPipes : public MegaCmdShellCommunications
{
private:
    bool redirectedstdout;
public:

    MegaCmdShellCommunicationsNamedPipes();
    MegaCmdShellCommunicationsNamedPipes(bool _redirectedstdout):redirectedstdout(_redirectedstdout){};

    ~MegaCmdShellCommunicationsNamedPipes();

    virtual int executeCommand(std::string command, std::string (*readresponse)(const char *) = NULL, OUTSTREAMTYPE &output = COUT, bool interactiveshell = true, std::wstring = L"");
    virtual int executeCommandW(std::wstring command, std::string (*readresponse)(const char *) = NULL, OUTSTREAMTYPE &output = COUT, bool interactiveshell = true);

    virtual bool registerForStateChangesImpl(bool interactive, StateChangedCb_t statechangehandle, bool initiateServer = true) override;

    void setResponseConfirmation(bool confirmation);

    static HANDLE doOpenPipe(std::wstring nameOfPipe);

private:
    static bool namedPipeValid(HANDLE namedPipe);
    static void closeNamedPipe(HANDLE namedPipe);

    virtual int listenToStateChanges(int receiveNamedPipeNum, StateChangedCb_t statechangehandle) override;

    static bool confirmResponse;

    static HANDLE createNamedPipe(int number = 0,bool initializeserver = true);

    static bool isFileOwnerCurrentUser(HANDLE hFile);

};

}//end namespace
#endif
#endif // MEGACMDSHELLCOMMUNICATIONS_H
