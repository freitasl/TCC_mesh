#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/itu-r-1411-los-propagation-loss-model.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/integer.h"
#include "ns3/wave-bsm-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/core-module.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-classifier.h"
#include "ns3/ipv4-flow-classifier.h"

#include "ns3/gnuplot.h"
#include <string>
#include <cassert>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("mrmc");

class MeshTest
	

 {
  public:
  MeshTest ();
  void Configure (int argc, char ** argv);
  int Run ();
 private:
  int       m_xSize; 
  int       m_ySize; 
  double    m_step; 
  double    m_randomStart; 
  double    m_totalTime; 
  double    m_packetInterval;
  uint16_t m_packetSize;
  uint32_t m_nIfaces;
  bool     m_chan;
  bool     m_pcap;
  //bool enableFlowMonitor = true;
  std::string m_stack;
  std::string m_root;
  
  NodeContainer nodes;

  NetDeviceContainer meshDevices;


  Ipv4InterfaceContainer interfaces;

  MeshHelper mesh;
private:
	/// Criar nós e configurar sua mobilidade
	void CreateNodes ();
	/// Instalar internet m_stack nos nós
	void InstallInternetStack ();
	/// Instalar Aplicações
	void InstallApplication ();
	/// Exibe diagnósticos dos dispositivos mesh
	void Report ();
};
MeshTest::MeshTest ():
  m_xSize (3), m_ySize (3), //7*7  grade de nós
  m_step (100.0), // Distancia entre os nós será de 25 metros
  m_randomStart (0.1),
  m_totalTime (100.0),
  m_packetInterval (5.0),
  m_packetSize (1024),
  m_nIfaces (2),
  m_chan (true),
  m_pcap(false),
  m_stack ("ns3::Dot11sStack"),
  m_root ("ff:ff:ff:ff:ff:ff")
{
}
void
MeshTest::Configure (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("x-size", "Number of nodes in a row grid. [6]", m_xSize);
  cmd.AddValue ("y-size", "Number of rows in a grid. [6]", m_ySize);
  cmd.AddValue ("step", "Size of edge in our grid, meters. [100 m]", m_step);
  cmd.AddValue ("start", "Maximum random start delay, seconds. [0.1 s]", m_randomStart);
  cmd.AddValue ("time", "Simulation time, seconds [100 s]", m_totalTime);
  cmd.AddValue ("packet-interval", "Interval between packets in UDP ping, seconds [0.001 s]", m_packetInterval);
  cmd.AddValue ("packet-size", "Size of packets in UDP ping", m_packetSize);
  cmd.AddValue ("interfaces", "Number of radio interfaces used by each mesh point. [1]", m_nIfaces);
  cmd.AddValue ("channels", "Use different frequency channels for different interfaces. ", m_chan);
  cmd.AddValue ("pcap", "Enable PCAP traces on interfaces. [0]", m_pcap);
  cmd.AddValue ("stack", "Type of protocol stack. ns3::Dot11sStack by default", m_stack);
  cmd.AddValue ("root", "Mac address of root mesh point in HWMP", m_root);

  cmd.Parse (argc, argv);
  NS_LOG_DEBUG ("Grid:" << m_xSize << "*" << m_ySize);
  NS_LOG_DEBUG ("Simulation time:" << m_totalTime << " s");
}
void
MeshTest::CreateNodes ()
{
  /*
   * Create x*Y stattions to form a grid topology
   */
  nodes.Create (m_ySize*m_xSize);
  // Configure YansWifiChannel
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  mesh = MeshHelper::Default ();
  if (!Mac48Address (m_root.c_str ()).IsBroadcast ())
     {
       mesh.SetStackInstaller (m_stack, "Root", Mac48AddressValue (Mac48Address (m_root.c_str ())));
     }
  else
     {
       mesh.SetStackInstaller (m_stack);
     }
  if (m_chan)
     {
       mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
     }
  else
     {
      mesh.SetSpreadInterfaceChannels (MeshHelper::ZERO_CHANNEL); 
     }
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (m_randomStart)));

  mesh.SetNumberOfInterfaces (m_nIfaces);

  meshDevices = mesh.Install (wifiPhy, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
				 "MinX", DoubleValue (0.0),
				 "MinY", DoubleValue (0.0),
				 "DeltaX", DoubleValue (m_step),
				 "DeltaY", DoubleValue (m_step),
				 "GridWidth", UintegerValue (m_xSize),
				 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  if (m_pcap)
    wifiPhy.EnablePcapAll (std::string ("mp-"));
}
void
MeshTest::InstallInternetStack ()
{
  InternetStackHelper internetStack;
  internetStack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (meshDevices);
}
void
MeshTest::InstallApplication ()
{
 UdpEchoServerHelper echoServer (9);
 ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
 serverApps.Start (Seconds (0.0));
 serverApps.Stop (Seconds (m_totalTime));
 UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
 echoClient.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(m_totalTime*(1/m_packetInterval))));
 echoClient.SetAttribute ("Interval", TimeValue (Seconds (m_packetInterval)));
 echoClient.SetAttribute ("PacketSize", UintegerValue (m_packetSize));
 ApplicationContainer clientApps = echoClient.Install (nodes.Get (m_xSize*m_ySize-1));
 clientApps.Start (Seconds (0.0));
 clientApps.Stop (Seconds (m_totalTime));
}
int
MeshTest::Run ()
{
 CreateNodes ();
 InstallInternetStack ();
 InstallApplication ();
 Simulator::Schedule (Seconds (m_totalTime), &MeshTest::Report, this);
 Simulator::Stop (Seconds (m_totalTime));

 AnimationInterface anim ("mesh.xml");
 for (uint32_t i = 0; i < nodes.GetN (); ++i)
     {
	anim.UpdateNodeDescription (nodes.Get (i), "");
  	anim.UpdateNodeColor (nodes.Get (i), 0, 250, 0);
     }
  anim.EnableIpv4RouteTracking ("meshtrace.xml", Seconds (0), Seconds (5), Seconds (0.25));
  anim.EnableWifiMacCounters (Seconds (0), Seconds (10));
  anim.EnableWifiPhyCounters (Seconds (0), Seconds (10));



// Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds ( m_totalTime));
// Simulator::Stop (Seconds (300.0));
 Simulator::Run ();



monitor->CheckForLostPackets ();
Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

	uint32_t txPacketsum = 15;
	uint32_t rxPacketsum = 10;
	uint32_t DropPacketsum = 10;
	uint32_t LostPacketsum = 10;
	uint32_t rxBytessum = 10;
	double Delaysum = 0;
std::ofstream ofs ("ResultGraph.plt", std::ofstream::out);


for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
  {
		txPacketsum += iter->second.txPackets;
		rxPacketsum += iter->second.rxPackets;
		LostPacketsum += iter->second.lostPackets;
		DropPacketsum += iter->second.packetsDropped.size();
		Delaysum += iter->second.delaySum.GetSeconds();
		rxBytessum += iter->second.rxBytes;

	NS_LOG_UNCOND("Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << " mbps");
	NS_LOG_UNCOND("Tx Packets: " << iter->second.txPackets);
	NS_LOG_UNCOND("Rx Packets> " << iter->second.rxPackets);
	NS_LOG_UNCOND("Delay: " << iter->second.delaySum.GetSeconds());
	// NS_LOG_UNCOND("Lost Packets: " << iter->second.lostPackets);
 	// NS_LOG_UNCOND("Drop Packets: " << iter->second.packetDropped.size());
	// NS_LOG_UNCOND("Packets Delivery Ratio: " << ((iter->second.rxPackets * 100) / iter->second.txPackets) << "%");
	// NS_LOG_UNCOND("PAckets Lost Ratio: " << ((iter->second.lostPackets * 100) / iter->second.txPackets) << "%" << "\n");

	  monitor->SerializeToXmlFile("lablab.flowmon1.xml", true, true);

  }

  NS_LOG_UNCOND("Average PDR: " << ((rxPacketsum * 100) / txPacketsum) << " ");
  NS_LOG_UNCOND("Average jitter: " << ((LostPacketsum * 100) / txPacketsum) << "");
  NS_LOG_UNCOND("Average Throughput: " << ((rxBytessum * 8.0) / ( m_totalTime)) / 1024 / 4 << " mbps");
  NS_LOG_UNCOND("Average Delay: " << (Delaysum / rxPacketsum) * 1000 << " ms" << "\n");

ofs << "set terminal png" <<std::endl;
ofs << "set output 'ResultGraph.png'"<<std::endl;
ofs << "set title ''" <<std::endl;
ofs << "set xlabel 'Nodes' " <<std::endl;
ofs << "set ylabel 'value' " <<std::endl;
ofs << "plot "<<" '-'" <<"title "<<"'packet_inter_arrival_time(ms)' with linespoints, '-' title 'jitter' with lines,'-' title 'Throughput' with lines, '-' title 'delay' with lines" <<std::endl;
ofs <<"1 " <<0<<std::endl;
ofs << (m_xSize* m_ySize)<<" " <<Seconds (m_packetInterval)/(10000*10000)<<std::endl;
ofs << "e"<<std::endl;
ofs <<"1 " <<0<<std::endl;
ofs << (m_xSize* m_ySize)<<" " <<((LostPacketsum * 100) / txPacketsum)<<std::endl;
ofs << "e"<<std::endl;
ofs <<"1 " <<0<<std::endl;
ofs << (m_xSize* m_ySize)<<" " <<((rxBytessum * 8.0) / ( m_totalTime)) / 1024 / 4 <<std::endl;
ofs << "e"<<std::endl;

ofs <<"1 " <<0<<std::endl;
ofs << (m_xSize* m_ySize)<<" " <<((LostPacketsum * 100) / txPacketsum)<<std::endl;
ofs << "e"<<std::endl;
ofs.close();

  Simulator::Destroy ();
  return 0;
}
  void

 MeshTest::Report ()
  {
  unsigned n (0);
    for (NetDeviceContainer::Iterator i = meshDevices.Begin (); i != meshDevices.End (); ++i, ++n)
      {
        std::ostringstream os;
        os << "mp-report-" << n << ".xml";
         std::cerr << "Printing mesh point device #" << n << " diagnostics to " << os.str () << "\n";
         std::ofstream of;
        of.open (os.str ().c_str ());
        if (!of.is_open ())
         {
            std::cerr << "Error: Can't open file " << os.str () << "\n";
             return;
          }
        mesh.Report (*i, of);
         of.close ();
      }
  }
   int
   main (int argc, char *argv[])
  {
    MeshTest t; 
    t.Configure (argc, argv);
    return t.Run ();
  }


	



   


