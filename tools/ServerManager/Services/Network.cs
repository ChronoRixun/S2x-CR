using System;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The address other machines reach this box on. The master heartbeat does not hand the
    /// public address back, so this is the machine's own IPv4: right on a LAN, and the thing a
    /// host behind a router swaps for their public IP before they paste it.
    /// </summary>
    internal static class Network
    {
        private static string _cached;
        private static DateTime _read;

        public static string LocalAddress()
        {
            // A cable pulled out or a VPN coming up changes this, so it is worth re-reading,
            // but not on every card that draws.
            if (_cached != null && (DateTime.Now - _read).TotalSeconds < 60) return _cached;
            _read = DateTime.Now;
            _cached = FirstAddress();
            return _cached;
        }

        private static string FirstAddress()
        {
            try
            {
                var candidates =
                    from card in NetworkInterface.GetAllNetworkInterfaces()
                    where card.OperationalStatus == OperationalStatus.Up
                       && card.NetworkInterfaceType != NetworkInterfaceType.Loopback
                       && card.NetworkInterfaceType != NetworkInterfaceType.Tunnel
                    from address in card.GetIPProperties().UnicastAddresses
                    where address.Address.AddressFamily == AddressFamily.InterNetwork
                       && !IPAddress.IsLoopback(address.Address)
                       && !address.Address.ToString().StartsWith("169.254", StringComparison.Ordinal)
                    select address.Address.ToString();
                return candidates.FirstOrDefault();
            }
            catch
            {
                // No adapter readable is not an error worth a dialog: the loopback line still works.
                return null;
            }
        }
    }
}
