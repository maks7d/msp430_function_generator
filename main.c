#include <msp430.h> // Header principal qui va inclure msp430g2553.h

int main(void) {
    // 1. Arrête le Watchdog Timer (sinon le MCU redémarre en boucle)
    WDTCTL = WDTPW | WDTHOLD; 

    // 2. Configurer l'horloge système (DCO) à 1 MHz (stable et calibré)
    DCOCTL = 0; // S'assure de partir d'un état propre
    BCSCTL1 = CALBC1_1MHZ; // Plage de l'oscillateur pour 1 MHz
    DCOCTL = CALDCO_1MHZ;  // Modulateur précis pour 1 MHz

    // 3. Configurer la broche de sortie matérielle (P1.2)
    P1DIR |= BIT2;  // Direction: Sortie ("1") pour la broche P1.2
    P1SEL |= BIT2;  // Sélection: Active la fonction spéciale (connecte P1.2 au Timer TA0.1)

    // 4. Configuration de la base de temps du Timer A0
    // TASSEL_2 = Source d'horloge SMCLK (ici branchée sur le 1 MHz)
    // MC_1     = Mode "Up" (le timer compte de 0 jusqu'à la valeur de TA0CCR0 puis boucle)
    TA0CTL = TASSEL_2 + MC_1; 
    
    // 5. Définition de la fréquence du signal (100 Hz)
    // Formule: Horloge / Fréquence voulue = 1 000 000 / 100 = 10 000 cycles d'horloge.
    // On retire 1 car le MCU compte le "zéro" aussi (0 à 9999 = 10000 étapes).
    TA0CCR0 = 10000 - 1; 
    
    // 6. Définition du rapport cyclique (50% pour un carré parfait)
    // Le signal changera d'état matériellement à la moitié du compte (5000 cycles)
    TA0CCR1 = 5000; 
    
    // 7. Choix de l'action matérielle sur la broche
    // OUTMOD_7 = Mode "Reset/Set". 
    // Quand le timer atteint CCR1 (5000), la broche P1.2 passe à 0V (Reset).
    // Quand le timer boucle à zéro (après 9999), la broche P1.2 repasse à 3.3V (Set).
    TA0CCTL1 = OUTMOD_7; 

    // 8. Économie d'énergie maximale
    // On met le CPU en sommeil profond niveau 0 (LPM0)
    // L'oscillateur 1 MHz et le Timer continuent de tourner tous seuls !
    __bis_SR_register(LPM0_bits); 
    
    return 0;
}