/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h" // for debugging using netanim
#include "ns3/gnuplot.h" // to create plots using GNUPlot.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WNsimulation");

double throughput[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
* Function simulation
* @param datarate_bb: the datarate from the wifi router to the backbone.
* Returns a vector of doubles of size 3 containing the throughput of the 3 wifi stations.
*/
//double* 
void simulation(std::string datarate_bb, uint32_t nWifi, int packet_size, std::string streaming_rate, int sim_type, bool verbose)
{
    // Disable CTS/RTS if sim_type equals 3 or 5, enable otherwise
  UintegerValue ctsThr;
  if (sim_type == 3 || sim_type == 5)
  {
    ctsThr = (UintegerValue (2200)); 
  }
  else
  {
    ctsThr = (UintegerValue (100)); 
  }
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);

  if (verbose)
  {
    LogComponentEnable ("WNsimulation", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  // Create a point to point (p2p) connection from the wifi router to the backbone
  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (datarate_bb)); // set the backbone datarate
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms")); // set the backbone delay

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set ("RxGain", DoubleValue (0) );
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // Assign constant mobility (no movement) to the access point and backbone
  mobility.Install (p2pNodes);

  // Install the internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (p2pNodes);


  // Assign the IPv4 address to the p2p nodes
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  // Assign the IPv4 addresses to the wifi nodes
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces;
  wifiInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create the OnOff application to send UDP datagrams of size
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (wifiInterfaces.GetAddress (0), port)));
  onoff.SetAttribute ("PacketSize", UintegerValue (packet_size));
  // set parameters to saturate the channel
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  if (sim_type == 1){
    onoff.SetConstantRate (DataRate ("448kb/s"));
  }
  else if (sim_type == 2 || sim_type == 3){
    onoff.SetConstantRate (DataRate ("5Mb/s"));
  }
  else if (sim_type == 4 || sim_type == 5){
    onoff.SetConstantRate (DataRate (streaming_rate));
  }
  else {
    // clean up the simulator
    Simulator::Destroy ();
    return;
  }

  double start_time = 1.0;
  double end_time = 10.0;

  ApplicationContainer apps = onoff.Install (p2pNodes.Get (1));
  apps.Start (Seconds (start_time));
  apps.Stop (Seconds (end_time));
  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  apps = sink.Install (wifiStaNodes.Get (0));
  apps.Start (Seconds (start_time));
  apps.Stop (Seconds (end_time));

  if (sim_type == 4 || sim_type == 5){
    // lower the datarates for all other stations to 448kb/s
    onoff.SetAttribute("DataRate", StringValue("448kb/s"));
  }

  for (uint32_t i = 1; i < nWifi; i++)
  {
    // Create a similar flow for all nodes but each, 
    // starting and ending 0.1 seconds later than the previous 
    // one to avoid collisions at the start.
    onoff.SetAttribute ("Remote",
                        AddressValue (InetSocketAddress (wifiInterfaces.GetAddress (i), port)));
    apps = onoff.Install (p2pNodes.Get (1));
    apps.Start (Seconds (start_time + i*0.1));
    apps.Stop (Seconds (end_time + i*0.1));
    // Create a packet sink to receive these packets
    apps = sink.Install (wifiStaNodes.Get (i));
    apps.Start (Seconds (start_time + i*0.1));
    apps.Stop (Seconds (end_time + i*0.1));
  }

  Simulator::Stop (Seconds (12.2));

  NS_LOG_INFO ("install FlowMonitor on all nodes");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // run the simulator
  Simulator::Run ();

  NS_LOG_INFO ("Extract flow statistics of every node");
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  int j = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    // Duration for throughput measurement is 9.0 seconds, since
    if (i->first > 0)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      throughput[j] = i->second.rxBytes * 8.0 / (end_time - start_time) / 1000 / 1000;
      j++;
    }
  }
  // clean up the simulator
  Simulator::Destroy ();
  return;
}


/*
* ====================================
*         MAIN FUNCTION
* ====================================
*/
int main (int argc, char *argv[])
{
	int datarate = 500;
	std::stringstream ss;
  int DR_start = 1; //DR = datarate
  int DR_stepsize = 100;
  int DR_no_steps = 20;
  uint32_t nWifi = 3; // Number of wifi stations
  uint32_t nWifiMax = 20; // Maximum number of wifi stations
  bool verbose = false;
  int packet_size = 1400; // size of the UDP packets

  /*
  * =========================================
  *   PLOT THROUGPUT VS. BACKBONE DATARATE
  * =========================================
  */
  NS_LOG_INFO ("Starting first simulation.");
  std::string fileNameWithNoExtension = "throughput-vs-backbone-datarate";
  std::string graphicsFileName        = fileNameWithNoExtension + ".png";
  std::string plotFileName            = fileNameWithNoExtension + ".plt";
  std::string plotTitle               = "Throughput vs. backbone datarate";

  int sim_type = 1;

  // Instantiate the datasets, set their titles, and make sure the points are
  // plotted along with connecting lines.
  Gnuplot2dDataset flow1;
  flow1.SetTitle ("Throughput vs. backbone datarate");
  flow1.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset flow2;
  flow2.SetTitle ("Saturation for all nodes");

  Gnuplot2dDataset flow3;
  flow3.SetTitle ("Saturation throughput of each node");

  double x;
  double y;

  for(int i=0; i<DR_no_steps; i++){
    datarate = DR_start + DR_stepsize*i;
    ss.str("");
    ss << datarate << "kbps";
  	std::string datarate_bb = ss.str();
  	std::cout<< "\n" << "Datarate: " << datarate_bb << "\n";

    simulation(datarate_bb, nWifi, packet_size, "none", sim_type, verbose);
    double sum = 0;
    for (uint32_t i = 0; i < nWifi; i++)
    {
      std::cout<< "throughput " << i << ": " << throughput[i] << "\n";
      sum += throughput[i];
    }
    double average = sum/nWifi;

    // Add datapoints to the datasets.
    x = datarate;
    y = average;
    flow1.Add (x, y);
  }

  // Add vertical line to indicate the break-even point in bandwidth
  x = 3*472.32;
  y = 0;
  flow2.Add (x, y);
  y = 0.55;
  flow2.Add (x, y);

  // Add vertical line to indicate the break-even point in throughput
  x = 0;
  y = 0.47232;
  flow3.Add (x, y);
  x = 2000;
  flow3.Add (x, y);

  // Instantiate the plot and set its title.
  Gnuplot plot (graphicsFileName);
  plot.SetTitle (plotTitle);

  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal ("png");

  // Set the labels for each axis.
  plot.SetLegend ("Backbone datarate (kbps)", "Average throughput (Mbps)");

  // Set the range for the x axis.
  ss.str("");
  ss << "set xrange [" << DR_start << ":" << DR_start+DR_stepsize*DR_no_steps << "]";
  std::string x_range = ss.str();
  plot.AppendExtra (x_range);

  // Set the range for the y axis.
  ss.str("");
  ss << "set yrange [" << 0 << ":" << 0.7 << "]";
  std::string y_range = ss.str();
  plot.AppendExtra (y_range);

  // Add the datasets to the plot.
  plot.AddDataset (flow1);
  plot.AddDataset (flow2);
  plot.AddDataset (flow3);

  // Open the plot file.
  std::ofstream plotFile (plotFileName.c_str());

  // Write the plot file.
  plot.GenerateOutput (plotFile);

  // Close the plot file.
  plotFile.close ();

  #if 1
  /*
  * ============================================================
  *   PLOT AVERAGE THROUGPUT VS. NUMBER OF STATIONS AND RTS/CTS
  * ============================================================
  */
  std::cout<< "\n\n";
  NS_LOG_INFO ("Starting second simulation.");

  fileNameWithNoExtension = "throughput-vs-numOfStations";
  graphicsFileName        = fileNameWithNoExtension + ".png";
  plotFileName            = fileNameWithNoExtension + ".plt";
  plotTitle               = "Average throughput vs. number of stations";  

  // Instantiate the datasets, set their titles, and make sure the points are
  // plotted along with connecting lines.
  Gnuplot2dDataset dataset1;
  dataset1.SetTitle ("Average throughput with RTS/CTS");
  dataset1.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset2;
  dataset2.SetTitle ("Average throughput without RTS/CTS");
  dataset2.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset3;
  dataset3.SetTitle ("Total throughput with RTS/CTS");
  dataset3.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset4;
  dataset4.SetTitle ("Total throughput without RTS/CTS");
  dataset4.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  // run simulation with RTS/CTS
  sim_type = 2;
  for(uint32_t i=1; i <= nWifiMax; i++){
    std::cout<< "\n";
    std::string datarate_bb = "11Mbps";

    simulation(datarate_bb, i, packet_size, "none", sim_type, verbose);
    double sum = 0;
    for (uint32_t j = 0; j < i; j++)
    {
      std::cout<< "throughput " << j << ": " << throughput[j] << "\n";
      sum += throughput[j];
    }
    double average = sum/i;

    // Add datapoint to the dataset.
    x = i;
    y = average;
    dataset1.Add (x, y);
    y = sum;
    dataset3.Add(x, y);
  }

  // run simulation without RTS/CTS
  sim_type = 3;
  for(uint32_t i=1; i <= nWifiMax; i++){
    std::cout<< "\n";
    std::string datarate_bb = "11Mbps";

    simulation(datarate_bb, i, packet_size, "none", sim_type, verbose);
    double sum = 0;
    for (uint32_t j = 0; j < i; j++)
    {
      std::cout<< "throughput " << j << ": " << throughput[j] << "\n";
      sum += throughput[j];
    }
    double average = sum/i;

    // Add datapoint to the dataset.
    x = i;
    y = average;
    dataset2.Add (x, y);
    y = sum;
    dataset4.Add(x, y);
  }  

  // Instantiate the plot and set its title.
  Gnuplot plot1 (graphicsFileName);
  plot1.SetTitle (plotTitle);

  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot1.SetTerminal ("png");

  // Set the labels for each axis.
  plot1.SetLegend ("Number of stations", "Throughput (Mbps)");

  // Set the range for the x axis.
  ss.str("");
  ss << "set xrange [" << 1 << ":" << nWifiMax << "]";
  x_range = ss.str();
  plot1.AppendExtra (x_range);
  // Set the range for the y axis.
  ss.str("");
  ss << "set yrange [" << 0 << ":" << 5.5 << "]";
  y_range = ss.str();
  plot1.AppendExtra (y_range);

  // Add the dataset to the plot.
  plot1.AddDataset (dataset1);
  plot1.AddDataset (dataset2);
  plot1.AddDataset (dataset3);
  plot1.AddDataset (dataset4);

  // Open the plot file.
  std::ofstream plotFile1 (plotFileName.c_str());

  // Write the plot file.
  plot1.GenerateOutput (plotFile1);

  // Close the plot file.
  plotFile1.close ();
  #endif

  #if 1
  /*
  * ============================================================
  *   PLOT AVERAGE THROUGPUT VS. BIG STREAMER
  * ============================================================
  */
  std::cout<< "\n\n";
  NS_LOG_INFO ("Starting third simulation.");

  fileNameWithNoExtension = "throughput-vs-streamer";
  graphicsFileName        = fileNameWithNoExtension + ".png";
  plotFileName            = fileNameWithNoExtension + ".plt";
  plotTitle               = "Average throughput vs. a streamer's datarate";

  int numOfStations = 5; // Number of wifi stations in this simulation
  std::string streaming_rate[5] = {"1Mbps", "2Mbps", "3Mbps", "4Mbps", "5Mbps"};

  // Instantiate the datasets, set their titles, and make sure the points are
  // plotted along with connecting lines.
  Gnuplot2dDataset dataset5;
  dataset5.SetTitle ("Average throughput non-streamers with RTS/CTS");
  dataset5.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset6;
  dataset6.SetTitle ("Average throughput non-streamers without RTS/CTS");
  dataset6.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset7;
  dataset7.SetTitle ("Total throughput with RTS/CTS");
  dataset7.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset dataset8;
  dataset8.SetTitle ("Total throughput without RTS/CTS");
  dataset8.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  // run simulation with RTS/CTS
  sim_type = 4;
  for(int i=0; i < 5; i++){
    std::cout<< "\n";
    std::string datarate_bb = "11Mbps";

    simulation(datarate_bb, numOfStations, packet_size, streaming_rate[i], sim_type, verbose);
    double sum = 0;
    double max = 0;
    for (int j = 0; j < numOfStations; j++)
    {
      std::cout<< "throughput " << j << ": " << throughput[j] << "\n";
      if (max < throughput[j])
      {
        max = throughput[j];
      }
      sum += throughput[j];
    }
    double average = (sum - max) / (numOfStations - 1);

    // Add datapoint to the dataset.
    x = i+1;
    y = average;
    dataset5.Add (x, y);
    y = sum;
    dataset7.Add(x, y);
  }

  // run simulation without RTS/CTS
  sim_type = 5;
  for(int i=0; i < 5; i++){
    std::cout<< "\n";
    std::string datarate_bb = "11Mbps";

    simulation(datarate_bb, numOfStations, packet_size, streaming_rate[i], sim_type, verbose);
    double sum = 0;
    double max = 0;
    for (int j = 0; j < numOfStations; j++)
    {
      std::cout<< "throughput " << j << ": " << throughput[j] << "\n";
      if (max < throughput[j])
      {
        max = throughput[j];
      }
      sum += throughput[j];
    }
    double average = (sum - max) / (numOfStations - 1);

    // Add datapoint to the dataset.
    x = i+1;
    y = average;
    dataset6.Add (x, y);
    y = sum;
    dataset8.Add(x, y);
  }

  // Instantiate the plot and set its title.
  Gnuplot plot2 (graphicsFileName);
  plot2.SetTitle (plotTitle);

  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot2.SetTerminal ("png");

  // Set the labels for each axis.
  plot2.SetLegend ("Streamer's datarate (Mbps)", "Throughput (Mbps)");

  // Set the range for the x axis.
  ss.str("");
  ss << "set xrange [" << 1 << ":" << 5 << "]";
  x_range = ss.str();
  plot2.AppendExtra (x_range);
  // Set the range for the y axis.
  ss.str("");
  ss << "set yrange [" << 0 << ":" << 5 << "]";
  y_range = ss.str();
  plot2.AppendExtra (y_range);

  // Add the dataset to the plot.
  plot2.AddDataset (dataset5);
  plot2.AddDataset (dataset6);
  plot2.AddDataset (dataset7);
  plot2.AddDataset (dataset8);

  // Open the plot file.
  std::ofstream plotFile2 (plotFileName.c_str());

  // Write the plot file.
  plot2.GenerateOutput (plotFile2);

  // Close the plot file.
  plotFile2.close ();
  #endif

  return 0;
}