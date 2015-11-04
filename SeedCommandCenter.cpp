//
// Created by evshiron on 11/3/15.
//

#include <iostream>
#include <functional>

#include "SeedCommandCenter.h"
#include "SeedPacket.h"

#define FATAL(x) { cerr << x << endl; exit(1); }

SeedCommandCenter::SeedCommandCenter(const char* dev, SeedConfig& config) {

    mErrbuf = new char[PCAP_ERRBUF_SIZE];

    Handle = pcap_create(dev, mErrbuf);

    if(!Handle) FATAL("ERROR_PCAP_CREATE_FAILED");

}

void SeedCommandCenter::Start() {

    cout << "Start." << endl;

    mIsStopped = false;

    pcap_set_buffer_size(Handle, BUFSIZ);
    pcap_set_promisc(Handle, true);
    //pcap_set_rfmon(Handle, true);

    //pcap_setnonblock(Handle, true, mErrbuf);

    pcap_set_timeout(Handle, 1);

    pcap_activate(Handle);

    // FIXME:
    mListener = new thread([&]() {

        listen();

    });

}

void SeedCommandCenter::listen() {

    bpf_program filter;

    pcap_compile(Handle, &filter, R"(ether proto 0x1122)", true, PCAP_NETMASK_UNKNOWN);

    pcap_setfilter(Handle, &filter);

    pcap_pkthdr* header;
    const u_char* data;

    SeedPacket* packet;

    while(!mIsStopped) {

        //cout << "Loop." << endl;

        switch(pcap_next_ex(Handle, &header, &data)) {

            case 1:

                cout << "Pcap captured." << endl;

                packet = new SeedPacket(data);

                dispatchPacket(packet);

                break;

            case -1:

                cerr << pcap_geterr(Handle) << endl;
                pcap_perror(Handle, mErrbuf);
                cerr << mErrbuf << endl;

                FATAL("ERROR_PCAP_CAPTURE_FAILED");

                break;

            case 0:

                //cout << "Pcap capture timeout." << endl;
                break;

        }

    }

    pcap_close(Handle);

}

void SeedCommandCenter::Collect(SeedSession* session, char* tlvs) {

    for(int i = 0; i < 128 * 1024; i++) {

        if(tlvs[i] == 0 && tlvs[i+1] == 0 && tlvs[i+2] == 0) {

            break;

        }

        switch(tlvs[i]) {
            case 1:

                cout << "No (" << (int) tlvs[i+1] <<  "): " << &tlvs[i+2] << endl;
                i += 1 + tlvs[i+1];

                break;

            case 2:

                cout << "Name (" << (int) tlvs[i+1] <<  "): " << &tlvs[i+2] << endl;
                i += 1 + tlvs[i+1];

                break;

            case 3:

                cout << "Faculty (" << (int) tlvs[i+1] <<  "): " << &tlvs[i+2] << endl;
                i += 1 + tlvs[i+1];

                break;

            case 0:
            default:

                FATAL("ERROR_COLLECT_UNEXPECTED");

        }

    }

}

void SeedCommandCenter::Stop() {

    mIsStopped = true;

    mListener->join();

}

void SeedCommandCenter::dispatchPacket(SeedPacket* packet) {

    if(Sessions.count(packet->SessionId) == 0) {

        Sessions[packet->SessionId] = new SeedSession(this, packet->SessionId);

    }

    SeedSession* session = Sessions[packet->SessionId];

    session->Consume(packet);

}
