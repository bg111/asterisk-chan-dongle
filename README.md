<a href="https://travis-ci.org/wdoekes/asterisk-chan-dongle">
  <img alt="Travis Build Status"
       src="https://api.travis-ci.org/wdoekes/asterisk-chan-dongle.svg"/>
</a>

chan\_dongle channel driver for Huawei UMTS cards
=================================================

WARNING:

This channel driver is in alpha stage.
I am not responsible if this channel driver will eat your money on
your SIM card or do any unpredicted things.

Please use a recent Linux kernel, 2.6.33+ recommended.
If you use FreeBSD, 8.0+ recommended.

This channel driver should work with the folowing UMTS cards:
* Huawei K3715
* Huawei E169 / K3520
* Huawei E155X
* Huawei E175X
* Huawei E261
* Huawei K3765

Check complete list in:
http://wiki.e1550.mobi/doku.php?id=requirements#list_of_supported_models

Before using the channel driver make sure to:
* Disable PIN code on your SIM card

Supported features:
* Place voice calls and terminate voice calls
* Send SMS and receive SMS
* Send and receive USSD commands / messages

Some useful AT commands:

    AT+CCWA=0,0,1                   #disable call-waiting
    AT+CFUN=1,1                     #reset dongle
    AT^CARDLOCK="<code>"            #unlock code
    AT^SYSCFG=13,0,3FFFFFFF,0,3     #modem 2G only, automatic search any band, no roaming
    AT^U2DIAG=0                     #enable modem function

Building:
----------

    $ ./bootstrap
    $ ./configure --with-astversion=13.7
    $ make

If you run a different version of Asterisk, you'll need to update the
`13.7` as appropriate, obviously.

If you did not `make install` Asterisk in the usual location and configure
cannot find the asterisk header files in `/usr/include/asterisk`, you may
optionally pass `--with-asterisk=PATH/TO/INCLUDE`.

Here is an example for the dialplan:
------------------------------------

**WARNING**: *This example uses the raw SMS message passed to System() directly.
No sane person would do that with untrusted data without escaping/removing the
single quotes.*

    [dongle-incoming]
    exten => sms,1,Verbose(Incoming SMS from ${CALLERID(num)} ${BASE64_DECODE(${SMS_BASE64})})
    exten => sms,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DONGLENAME} - ${CALLERID(num)}: ${BASE64_DECODE(${SMS_BASE64})}' >> /var/log/asterisk/sms.txt)
    exten => sms,n,Hangup()

    exten => ussd,1,Verbose(Incoming USSD: ${BASE64_DECODE(${USSD_BASE64})})
    exten => ussd,n,System(echo '${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} - ${DONGLENAME}: ${BASE64_DECODE(${USSD_BASE64})}' >> /var/log/asterisk/ussd.txt)
    exten => ussd,n,Hangup()

    exten => s,1,Dial(SIP/2001@othersipserver)
    exten => s,n,Hangup()

    [othersipserver-incoming]

    exten => _X.,1,Dial(Dongle/r1/${EXTEN})
    exten => _X.,n,Hangup

    exten => *#123#,1,DongleSendUSSD(dongle0,${EXTEN})
    exten => *#123#,n,Answer()
    exten => *#123#,n,Wait(2)
    exten => *#123#,n,Playback(vm-goodbye)
    exten => *#123#,n,Hangup()

    exten => _#X.,1,DongleSendSMS(dongle0,${EXTEN:1},"Please call me",1440,yes,"magicID")
    exten => _#X.,n,Answer()
    exten => _#X.,n,Wait(2)
    exten => _#X.,n,Playback(vm-goodbye)
    exten => _#X.,n,Hangup()

You can also use this:
----------------------

Call using a specific group:

    exten => _X.,1,Dial(Dongle/g1/${EXTEN})

Call using a specific group in round robin:

    exten => _X.,1,Dial(Dongle/r1/${EXTEN})

Call using a specific dongle:

    exten => _X.,1,Dial(Dongle/dongle0/${EXTEN})

Call using a specific provider name:

    exten => _X.,1,Dial(Dongle/p:PROVIDER NAME/${EXTEN})

Call using a specific IMEI:

    exten => _X.,1,Dial(Dongle/i:123456789012345/${EXTEN})

Call using a specific IMSI prefix:

    exten => _X.,1,Dial(Dongle/s:25099203948/${EXTEN})

How to store your own number:

    dongle cmd dongle0 AT+CPBS=\"ON\"
    dongle cmd dongle0 AT+CPBW=1,\"+123456789\",145


Other CLI commands:
-------------------

    dongle reset <device>
    dongle restart gracefully <device>
    dongle restart now <device>
    dongle restart when convenient <device>
    dongle show device <device>
    dongle show devices
    dongle show version
    dongle sms <device> number message
    dongle ussd <device> ussd
    dongle stop gracefully <device>
    dongle stop now <device>
    dongle stop when convenient <device>
    dongle start <device>
    dongle restart gracefully <device>
    dongle restart now <device>
    dongle restart when convenient <device>
    dongle remove gracefully <device>
    dongle remove now <device>
    dongle remove when convenient <device>
    dongle reload gracefully
    dongle reload now
    dongle reload when convenient

For reading installation notes please look to INSTALL file.


Gain control and Jitter buffer
--------------------------------


<img src="https://cloud.githubusercontent.com/assets/6702424/26686554/9253bc18-46ed-11e7-9bce-cad8e2396435.png" 
width="800px" height="" />


In order to perform good quality calls you will need to take care of:

* **Automatic gain control**:

chan_dongle does not control the gain of the audio stream it receive.
This result of Alice hearing Bob's voice loud and noisy.
It is possible to manually manage the gain in *dongle.conf* but
the better option is by far to apply automatic gain control with
the dialplan function AGC.


* **Jitter buffer**:

Since asterisk 12 it is no longer possible to enable Jitter buffer
in dongle.conf it has to be applied in the dialplan.
The lack of Jitter buffer result in severe loss in the transport
of the voice from Bob to Alice. 


#### Dialplan example

To set JITTERBUFFER and AGC in the dialplan on the appropriate channel
regardless of who is initiating the call we will have to use
the "b" option of Dial:

b( context^exten^priority )

Before initiating an outgoing call, Gosub to the specified
location using the newly created channel. 

The Gosub will be executed for each destination channel."

    [from-dongle]
    ; This will be executed by an indbound Dongle channel ( call initiated on the dongle side )
    exten => _[+0-9].,1,Dial(SIP/bob,b(from-dongle^outbound^1)) ;

    ; This will be executed by an outbound SIP channel ( channel generated by dial )
    exten => outbound,1,Set(JITTERBUFFER(adaptive)=default)
    same => n,Set(AGC(rx)=4000)
    same => n,Return()

    [from-sip]
    ; This will be executed by an inbound SIP channel ( call initiated on the SIP side )
    exten => _[+0-9].,1,Set(JITTERBUFFER(adaptive)=2000,1600,120)
    same => n,Set(AGC(rx)=4000)
    same => n,Dial(Dongle/i:${IMEI_OF_MY_DONGLE}/${NUMBER_OF_BOB}) 


Note: To use automatic gain control dialplan function (AGC) you will need
to compile Asterisk with func_speex ( see in menuselect ). 
On raspberry Pi you will need to compile and install speex and speexdsp yourself,
the version of speex provided by the depos does not support AGC.
(because compiled with fixed point instead of floating point)
see: [HOWTO](https://gist.github.com/garronej/01f0dac45efe9161969a83890c019efa)


For additional information about Huawei dongle usage look to
chan\_dongle Wiki at http://wiki.e1550.mobi and chan\_dongle project home at
https://github.com/wdoekes/asterisk-chan-dongle/
