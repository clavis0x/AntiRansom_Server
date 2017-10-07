#!/usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function
import gc
import os
import sys
import traceback
import shlex
import time
import re
import platform
from optparse import OptionParser
from pprint import pprint

g_fHasColors = True
g_dTermColors = {
    'red': '\033[31m',
    'blue': '\033[94m',
    'green': '\033[92m',
    'yellow': '\033[93m',
    'magenta': '\033[35m',
    'cyan': '\033[36m'
}


def pinfo(msg):
    print("[INFO] %s" % msg)


def perror(msg):
    print("[ERROR] %s" % msg)


def printErr(_ctx, e):
    oVBoxMgr = _ctx['global']
    if oVBoxMgr.errIsOurXcptKind(e):
        print(colored('%s: %s' % (oVBoxMgr.xcptToString(e), oVBoxMgr.xcptGetMessage(e)), 'red'))
    else:
        print(colored(str(e), 'red'))


def colored(strg, color):
    """
    Translates a string to one including coloring settings, if enabled.
    """
    if not g_fHasColors:
        return strg
    col = g_dTermColors.get(color, None)
    if col:
        return col + str(strg) + '\033[0m'
    return strg


def progressBar(ctx, progress, wait=1000):
    try:
        while not progress.completed:
            print("%s %%\r" % (colored(str(progress.percent), 'red')), end="")
            sys.stdout.flush()
            progress.waitForCompletion(wait)
            #ctx['global'].waitForEvents(0)
        if int(progress.resultCode) != 0:
            reportError(ctx, progress)
        return 1
    except KeyboardInterrupt:
        print("Interrupted.")
        ctx['interrupt'] = True
        if progress.cancelable:
            print("Canceling task...")
            progress.cancel()
        return 0


class VBoxAuto:
    def __init__(self, machine):
        self.machine = machine
        self.ctx = {}
        self.mach = None
        self.session = None

    def get_const(self, enum, elem):
        # this lookup fails on Python2.6 - if that happens
        # then just return the element number
        try:
            all = self.ctx['const'].all_values(enum)
            for e in all.keys():
                if str(elem) == str(all[e]):
                    return e
        except:
            return '%d' % elem

    def get_mach(self):
        return self.ctx['vb'].findMachine(self.machine)

    def check(self):
        #
        # Set up the shell interpreter context and start working.
        #
        from vboxapi import VirtualBoxManager
        oVBoxMgr = VirtualBoxManager(None, None)
        self.ctx = {
            'global': oVBoxMgr,
            'vb': oVBoxMgr.vbox,
            'const': oVBoxMgr.constants,
            'remote': oVBoxMgr.remote,
            'type': oVBoxMgr.type
        }

        vbox = self.ctx['vb']
        if vbox is not None:
            try:
                print("Running VirtualBox version %s" % (vbox.version))
            except Exception as e:
                printErr(self.ctx, e)
                if g_fVerbose:
                    traceback.print_exc()
            self.ctx['perf'] = None  # ctx['global'].getPerfCollector(vbox)
        else:
            self.ctx['perf'] = None

        self.ctx['perf'] = self.ctx['global'].getPerfCollector(self.ctx['vb'])
        self.mach = self.get_mach()
        if self.mach == None:
            perror('Cannot find the machine: %s' % self.machine)
            return False

        pinfo('Using %s (uuid: %s)' % (self.mach.name, self.mach.id))

        pinfo('Session state: %s' % self.get_const(
            "SessionState", self.mach.sessionState))
        pinfo('Machine state: %s' % self.get_const(
            "MachineState", self.mach.state))

        return True

    def startVm(self, nsecwait=20):
        vbox = self.ctx['vb']
        perf = self.ctx['perf']
        self.session = self.ctx['global'].getSessionObject(vbox)
        progress = self.mach.launchVMProcess(self.session, "gui", "") # headless
        if progressBar(self.ctx, progress, 100) and int(progress.resultCode) == 0:
            # we ignore exceptions to allow starting VM even if
            # perf collector cannot be started
            if perf:
                try:
                    perf.setup(['*'], [self.mach], 10, 15)
                except Exception as e:
                    printErr(self.ctx, e)
                    if g_fVerbose:
                        traceback.print_exc()
            self.session.unlockMachine()

    def cmdExistingVm(self, cmd, args):
        session = None
        try:
            vbox = self.ctx['vb']
            session = self.ctx['global'].getSessionObject(vbox)
            self.mach.lockMachine(session, self.ctx['global'].constants.LockType_Shared)
        except Exception as e:
            printErr(self.ctx, "Session to '%s' not open: %s" % (self.mach.name, str(e)))
            if g_fVerbose:
                traceback.print_exc()
            return
        if session.state != self.ctx['const'].SessionState_Locked:
            print("Session to '%s' in wrong state: %s" % (self.mach.name, session.state))
            session.unlockMachine()
            return

        console = session.console
        ops = {'pause': lambda: console.pause(),
               'resume': lambda: console.resume(),
               'powerdown': lambda: console.powerDown(),
               'powerbutton': lambda: console.powerButton()
               }
        try:
            ops[cmd]()
        except KeyboardInterrupt:
            ctx['interrupt'] = True
        except Exception as e:
            printErr(self.ctx, e)
            if g_fVerbose:
                traceback.print_exc()

        session.unlockMachine()

    def stopVm(self):
        self.cmdExistingVm('powerdown', '')

    def pauseVm(self):
        self.cmdExistingVm('pause', '')

    def resumeVm(self):
        self.cmdExistingVm('resume', '')

    def cmdAnyVm(ctx, mach, cmd, args=[], save=False):
        session = self.ctx['global'].openMachineSession(self.mach)
        self.mach = session.machine
        try:
            cmd(self.ctx, self.mach, session.console, args)
        except Exception as e:
            save = False
            printErr(self.ctx, e)
            if g_fVerbose:
                traceback.print_exc()
        if save:
            self.mach.saveSettings()
        self.ctx['global'].closeMachineSession(session)

    def restoreSnapshot(self, name):
        if self.mach is None:
            return 0
        session = self.ctx['global'].openMachineSession(self.mach)
        snap = self.mach.findSnapshot(name)
        console = session.console
        console.restoreSnapshot(snap)
        time.sleep(5)
        self.ctx['global'].closeMachineSession(session)

    def _connectToGuest(self, machine):
        guest = self.session.console.guest
        #Creating session with the guest VM
        guestSession = guest.createSession("SB-Win7x64", "", "", "")
        result = guestSession.waitFor(1, 10000)
        if (result != 1):
            print("Failed")
            return -1
        else:
            return guestSession

    #Lock the given machine to the current session
    def _lockMachine (self, mode):
        vbox = self.ctx['vb']
        mach = vbox.findMachine('Sandbox-Windows 7 x64')
        mach.lockMachine(self.session, mode)
        return 0

    def execInGuest(self):
        #Lock given machine to the current session
        lock = self._lockMachine(self.ctx['global'].constants.LockType_Shared)
        if (lock == 0):
            guestSession = self._connectToGuest(self.mach)
            if (guestSession != -1):
                guestProcess = guestSession.processCreate("C:\\run.bat", [], [], [], 0)
                #guestProcess = guestSession.processCreate("notepad.exe", [], [], [], 0)
                result = guestProcess.waitFor(2, 3000)
                guestSession.close() #Close guest session
            self.session.unlockMachine() #Unlock machine

def main(argv):
    print("Nothing to do. Import me!")
    return 0


if __name__ == '__main__':
    main(sys.argv)
