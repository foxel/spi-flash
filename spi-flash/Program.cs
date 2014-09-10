using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.IO.Ports;

namespace spi_flash
{

    class Program
    {
        private static byte[] buffer;
        private static SerialPort conn;

        static void Main(string[] args)
        {
            if (args.Length < 3)
            {
                Console.WriteLine("Not enough parameters");
            }
            else
            {
                try
                {
                    doFlash(args[0], args[1], args[2]);
                    Console.WriteLine("Complete");
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                }
            }

            //Console.ReadKey();
        }

        static string readString()
        {
            int len = 0;
            char c;

            while ((c = (char)conn.ReadChar()) != '\n')
            {
                buffer[len] = (byte) c;
                len++;
            }

            return Encoding.ASCII.GetString(buffer, 0, len);
        }

        static void checkEnd()
        {
            char c;
            if ((c = (char)conn.ReadChar()) != '\n')
            {
                throw new Exception("Unexpected response: " + c);
            }
        }

        static void doFlash(string fileName, string portName, string chipId)
        {
            FileStream f = File.OpenRead(fileName);
            buffer = new byte[300];
            conn = new SerialPort(portName, 57600);
            conn.Encoding = new ASCIIEncoding();
            conn.ReadTimeout = 5000;
            conn.Open();

            conn.Write("0");
            checkEnd();

            conn.Write("1");
            Console.WriteLine(readString());

            conn.Write("r");
            string curChipId = readString();
            Console.WriteLine(curChipId);

            if (curChipId != chipId) {
                throw new Exception("Chip ID incorrect");
            }

            int readPos = 0;
            int readBytes;

            conn.Write("C");
            checkEnd();
            
            while ((readBytes = f.Read(buffer, 5, 256)) > 0)
            {
                buffer[0] = (byte)'W';
                buffer[1] = (byte)(readPos >> 16);
                buffer[2] = (byte)(readPos >> 8);
                buffer[3] = (byte)(readPos & 0xff);
                buffer[4] = (byte)((readBytes - 1) & 0xff);

                conn.Write(buffer, 0, readBytes + 5);
                checkEnd();

                buffer[0] = (byte)'R';

                conn.Write(buffer, 0, 5);

                int errorPos = 0;
                Boolean hasError = false;
                for (int i = 0; i < readBytes; i++)
                {
                    if (!hasError && buffer[5 + i] != conn.ReadByte())
                    {
                        hasError = true;
                        errorPos = readPos + i;
                    }
                }
                checkEnd();

                if (hasError)
                {
                    throw new Exception("Error verifying byte " + errorPos);
                }

                readPos += readBytes;
                Console.Write('.');
            }
            Console.WriteLine();
        }

        static bool compareArrays<T>(T[] a1, T[] a2, int len)
        {
            if (ReferenceEquals(a1, a2))
                return true;

            if (a1 == null || a2 == null)
                return false;

            if (a1.Length < len || a2.Length < len)
                return false;

            EqualityComparer<T> comparer = EqualityComparer<T>.Default;
            for (int i = 0; i < len; i++)
            {
                if (!comparer.Equals(a1[i], a2[i])) return false;
            }
            return true;
        }
    }
}
