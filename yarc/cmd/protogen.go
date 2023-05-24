/*
Copyright © 2022 NAME HERE <EMAIL ADDRESS>
*/
package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"github.com/gmofishsauce/yarc/pkg/protogen"
)

// protogenCmd represents the protogen command
var protogenCmd = &cobra.Command{
	Use:   "protogen",
	Short: "Generate serial protocol definitions for host and firmware",
	Long: `Protogen creates files defining serial protocol constants for
both the host side ("yarc host") and the Arduino firmware that controls
the YARC Downloader/host interface. Files are generaed in "." and must
be manually placed in their respective build locations before recompiling
both the host software and Arduino firmware.`,

	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("protogen called")
		protogen.Generate()
	},
}

func init() {
	rootCmd.AddCommand(protogenCmd)

	// Here you will define your flags and configuration settings.

	// Cobra supports Persistent Flags which will work for this command
	// and all subcommands, e.g.:
	// protogenCmd.PersistentFlags().String("foo", "", "A help for foo")

	// Cobra supports local flags which will only run when this command
	// is called directly, e.g.:
	// protogenCmd.Flags().BoolP("toggle", "t", false, "Help message for toggle")
}
