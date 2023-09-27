/*
*/
package cmd

import (
	"github.com/spf13/cobra"

	"github.com/gmofishsauce/yarc/pkg/sim"
)

// simCmd represents the sim command
var simCmd = &cobra.Command{
	Use:   "sim [binFile]",
	Short: "The YARC simulator",
	Long: `Sim is the simulator for YARC. It accepts a yarc.bin
file ("./yarc.bin") and simulates with microcycle accuracy.
`,

	RunE: func(cmd *cobra.Command, args []string) error {
		return sim.Simulate(args);
	},
}

func init() {
	rootCmd.AddCommand(simCmd)
}
