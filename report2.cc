#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/error-model.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-star.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/int64x64-128.h"

#include <string>
#include <iostream>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("report2");


/*
 *
 * ./waf --run "scratch/report2 --useKernel=1 --tracing=1 --tcp_stack_nodeA=cubic --tcp_stack_nodeB=cubic --delayA=50ms --simTime=200 --err_rate=0.00001"
 *
 * Get stats with:
 * conversations
 * tshark -r report2-3-1.pcap -q -z conv,tcp > stats.txt
 * endpoints
 * tshark -r report2-3-1.pcap -q -z endpoints,tcp > stats.txt
 */


int
main (int argc, char *argv[])
{
    LogComponentEnable("report2", LOG_LEVEL_INFO);

    bool tracing = true;
    bool useKernel = true;

    Time simTime = Seconds (200.0);
    std::string dataRate = "5Mbps";
    std::string latency = "10ms";
    // bottleneck data rate and delay
    std::string bottleneck_bandwidth = "2Mbps";
    std::string bottleneck_delay = "200ms";
    std::string delayA = "10ms";
    std::string delayB = "10ms";
    std::string tcp_stack_nodeA = "";
    std::string tcp_stack_nodeB = "";
    // this is the default error rate of our link, that is, the probability of a single
    // byte being 'corrupted' during transfer.
    double err_rate = 0.000001;

    CommandLine cmd;
    cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue ("simTime", "length of simulation in seconds", simTime);
    cmd.AddValue ("useKernel", "Flag to enable NSC", bottleneck_delay);
    cmd.AddValue ("bandwidth", "Bottleneck bandwidth", bottleneck_bandwidth);
    cmd.AddValue ("delay", "Bottleneck delay", bottleneck_delay);
    cmd.AddValue ("delayA", "Bottleneck delay A --> R", delayA);
    cmd.AddValue ("delayB", "Bottleneck delay B --> R", delayB);
    cmd.AddValue ("err_rate", "Packet error rate. Probability of a single byte being corrupted.", err_rate);
    cmd.AddValue("tcp_stack_nodeA", "Which TCP stack to use on node A (vegas, cubic und reno (for NewReno)", tcp_stack_nodeA);
    cmd.AddValue("tcp_stack_nodeB", "Which TCP stack to use on node B (vegas, cubic und reno (for NewReno)", tcp_stack_nodeA);
    cmd.Parse (argc, argv);


    NodeContainer nodes;
    nodes.Create (4);

    // Install network stacks on the nodes
    InternetStackHelper stack;

    stack.Install (nodes.Get(2)); // no NSC for R
    if (useKernel) {
        // https://www.nsnam.org/doxygen/nsc-tcp-l4-protocol_8cc_source.html
        // same error in examples/tcp/tcp-nsc-comparison.cc
        stack.SetTcp ("ns3::NscTcpL4Protocol",
                              "Library", StringValue ("liblinux2.6.26.so"));
    }
    stack.Install (nodes.Get(0)); // A
    stack.Install (nodes.Get(1)); // B
    stack.Install (nodes.Get(3)); // C
    if (useKernel) {
        // cubic, vegas, reno
        // This uses ns-3s attribute system to set the 'net.ipv4.tcp_congestion_control' sysctl of the
        // stack.
        // The same mechanism could be used to e.g. disable TCP timestamps:
        // Config::Set ("/NodeList/*/$ns3::Ns3NscStack<linux2.6.26>/net.ipv4.tcp_timestamps", StringValue ("0"));
        if (tcp_stack_nodeA != "") {
            Config::Set ("/NodeList/0/$ns3::Ns3NscStack<linux2.6.26>/net.ipv4.tcp_congestion_control", StringValue (tcp_stack_nodeA));
        }

        if (tcp_stack_nodeB != "") {
            Config::Set ("/NodeList/1/$ns3::Ns3NscStack<linux2.6.26>/net.ipv4.tcp_congestion_control", StringValue (tcp_stack_nodeB));
        }

        if (tcp_stack_nodeA != "" && tcp_stack_nodeA == tcp_stack_nodeB) {
            Config::Set ("/NodeList/3/$ns3::Ns3NscStack<linux2.6.26>/net.ipv4.tcp_congestion_control", StringValue (tcp_stack_nodeA));
        }
    }

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifs;

    NetDeviceContainer allDevices;

    // Set up first link A ----> R
    PointToPointHelper pointToPoint;
    pointToPoint.SetChannelAttribute ("Delay", StringValue (delayA));
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
    NetDeviceContainer firstDevice = pointToPoint.Install (nodes.Get(0), nodes.Get(2));
    ifs.Add (address.Assign(firstDevice));
    allDevices.Add(firstDevice);
    address.NewNetwork();

    // Set up second link B ----> R
    pointToPoint.SetChannelAttribute ("Delay", StringValue (delayB));
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
    NetDeviceContainer secondDevice = pointToPoint.Install (nodes.Get(1), nodes.Get(2));
    ifs.Add (address.Assign(secondDevice));
    allDevices.Add(secondDevice);
    address.NewNetwork();

    // This is the bottleneck link
    // Set up third link R ----> C
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneck_bandwidth));
    bottleneckLink.SetChannelAttribute ("Delay", StringValue (bottleneck_delay));

    NetDeviceContainer thirdDevice = bottleneckLink.Install (nodes.Get(2), nodes.Get(3));
    ifs.Add (address.Assign(thirdDevice));
    allDevices.Add(thirdDevice);
    address.NewNetwork();

    // Configure the error model
    // Here we use RateErrorModel with packet error rate
    DoubleValue rate (err_rate);
    Ptr<RateErrorModel> em1 =
      CreateObjectWithAttributes<RateErrorModel> ("RanVar",
       StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"), "ErrorRate", rate);
    Ptr<RateErrorModel> em2 =
      CreateObjectWithAttributes<RateErrorModel> ("RanVar",
       StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"), "ErrorRate", rate);

    // This enables the specified errRate on both link endpoints of our bottleneck.
    thirdDevice.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));
    thirdDevice.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));


    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    for (uint16_t k = 0; k < ifs.GetN(); k++) {
        NS_LOG_INFO("Address on interface ifs.GetAddress(" << k <<", 0) is : " << ifs.GetAddress(k, 0));
    }

    uint16_t portApp1 = 50000;
    uint16_t portApp2 = 60000;

    OnOffHelper onOff = OnOffHelper ("ns3::TcpSocketFactory", Address ());
    onOff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

    AddressValue remoteAddress1 (InetSocketAddress (Ipv4Address (ifs.GetAddress(5, 0)), portApp1));
    onOff.SetAttribute ("Remote", remoteAddress1);
    onOff.SetAttribute ("PacketSize", StringValue ("1024"));
    onOff.SetAttribute ("DataRate", StringValue ("5Mbps"));
    onOff.SetAttribute ("StartTime", TimeValue (Seconds (0.5)));

    // Add First OnOff App on Node A
    ApplicationContainer clientApps = onOff.Install(nodes.Get(0));

    // Add Second OnOff App on Node B
    AddressValue remoteAddress2 (InetSocketAddress (Ipv4Address (ifs.GetAddress(5, 0)), portApp2));
    onOff.SetAttribute ("Remote", remoteAddress2);
    clientApps.Add( onOff.Install(nodes.Get(1)) );

    clientApps.Start (Seconds (1.0));
    clientApps.Stop (simTime);


    // Add Sink application
    Address sinkLocalAddress1 (InetSocketAddress (Ipv4Address::GetAny (), portApp1));
    PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkLocalAddress1);
    ApplicationContainer sinkApps = sinkHelper1.Install (nodes.Get(3));

    Address sinkLocalAddress2 (InetSocketAddress (Ipv4Address::GetAny (), portApp2));
    PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkLocalAddress2);
    sinkApps.Add (sinkHelper2.Install (nodes.Get(3)));

    sinkApps.Start (Seconds (0.5));
    sinkApps.Stop (simTime);

    //
    // Set up tracing if enabled
    //
    if (tracing)
    {
      AsciiTraceHelper ascii;
      //pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("report2.tr"));
      pointToPoint.EnablePcapAll ("report2", false);
    }

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (simTime);
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

    Ptr<PacketSink> pktsink;
    std::cout << "Total ";
    for (uint32_t i = 0; i < sinkApps.GetN(); i++)
      {
        pktsink = sinkApps.Get (i)->GetObject<PacketSink> ();
        std::cout << "Rx(" << i << ") = " << pktsink->GetTotalRx () <<
        " bytes" << std::endl;
      }
}
