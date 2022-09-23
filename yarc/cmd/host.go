/*
Copyright © 2022 NAME HERE <EMAIL ADDRESS>

*/
package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"github.com/gmofishsauce/yarc/pkg/host"
)

// hostCmd represents the host command
var hostCmd = &cobra.Command{
	Use:   "host",
	Short: "YARC serial line monitor",
	Long: `Host opens the serial line to the Arduino Nano which controls
the YARC Downloader. It issues POLL requests to service the Nano
and reads commands from the terminal to determine what it should
transmit. The Host command must be terminated to release the
serial port before a download of Nano firmware can be performed
from the Arduino GUI.`,

	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("host called")
		host.Main()
	},
}

func init() {
	rootCmd.AddCommand(hostCmd)
}
