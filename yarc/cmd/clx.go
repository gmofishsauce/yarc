/*
Copyright © 2022 NAME HERE <EMAIL ADDRESS>
*/
package cmd

import (
	"github.com/spf13/cobra"

	"github.com/gmofishsauce/yarc/pkg/clx"
)

// clxCmd represents the clx command
var clxCmd = &cobra.Command{
	Use:   "clx sourceFile",
	Short: "clx",
	Long: `clx`,

	Args: cobra.ExactArgs(1),
	Run: func(cmd *cobra.Command, args []string) {
		clx.Clx(args[0])
	},
}

func init() {
	rootCmd.AddCommand(clxCmd)
}
