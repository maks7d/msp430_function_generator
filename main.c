#include <msp430.h> // Header principal qui va inclure msp430g2553.h

// Variable globale pour stocker la période actuelle (en cycles d'horloge)
// 10000 = 100 Hz (avec une horloge de 1 MHz)
volatile unsigned int current_period = 10000;
volatile unsigned int current_duty_cycle = 50; // En pourcentage, ex: 50 pour 50%
volatile unsigned int current_mode = 0; // 0 = frequence, 1 = rapport cyclique

int main(void) {
    // 1. Arrête le Watchdog Timer
    WDTCTL = WDTPW | WDTHOLD; 

    // 2. Configurer l'horloge système (DCO) à 1 MHz
    DCOCTL = 0; 
    BCSCTL1 = CALBC1_1MHZ; 
    DCOCTL = CALDCO_1MHZ;  

    // 3. Configurer la broche de sortie matérielle (P1.2) pour le signal
    P1DIR |= BIT2;  
    P1SEL |= BIT2;  

    // 4. Configuration des broches pour l'encodeur rotatif (P1.3 = CLK, P1.4 = DT)
    P1DIR &= ~(BIT3 | BIT4 | BIT5); // Les définir comme entrées
    P1REN |= (BIT3 | BIT4 | BIT5);  // Activer les résistances internes
    P1OUT |= (BIT3 | BIT4 | BIT5);  // Configurer en Pull-Up (souvent nécessaire pour les encodeurs)
    
    // Configuration du bouton poussoir 
    P1DIR &= ~BIT5; // P1.5 en entrée
    P1REN |= BIT5;  // Activer la résistance interne
    P1OUT |= BIT5;  // Configurer en Pull-Up

    // *DÉBUG* : Configurer la LED rouge (P1.0) en sortie pour voir si l'interruption marche
    P1DIR |= BIT0;
    P1OUT &= ~BIT0; // Éteindre la LED au démarrage

    // Configuration des interruptions pour lire l'encodeur (sur CLK = P1.3)
    P1IE |= BIT3;            // Activer l'interruption sur P1.3
    P1IES |= BIT3;           // Déclencher sur le front descendant (quand le signal passe de 1 à 0)
    P1IFG &= ~BIT3;          // Effacer le drapeau d'interruption au cas où

    P1IE |= BIT5;            // Activer l'interruption sur P1.5 (switch du rotary encoder)
    P1IES |= BIT5;           // Déclencher sur le front descendant 
    P1IFG &= ~BIT5;          // Effacer le drapeau d'interruption au cas où

    //Configuration interruption pour le bouton poussoir
    P1IE |= BIT5;            // Activer l'interruption sur P1.5
    P1IES |= BIT5;           // Déclencher sur le front descendant (quand le bouton est pressé)
    P1IFG &= ~BIT5;          // Effacer le drapeau d'interruption au cas où

    // 5. Configuration de la base de temps du Timer A0
    TA0CTL = TASSEL_2 + MC_1; 
    
    // 6. Définition initiale de la fréquence et rapport cyclique (100 Hz, 50%)
    TA0CCR0 = current_period - 1; 
    TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; 
    TA0CCTL1 = OUTMOD_7; 

    // 7. Activer les interruptions globales
    __enable_interrupt();

    // 8. Économie d'énergie maximale
    // On met le CPU en sommeil profond (LPM0). 
    // Il se réveillera automatiquement juste le temps de traiter l'encodeur rotatif.
    __bis_SR_register(LPM0_bits); 
    
    return 0;
}

// ---------------------------------------------------------
// ROUTINE D'INTERRUPTION (ISR) DU PORT 1
// S'exécute automatiquement quand on tourne l'encodeur
// ---------------------------------------------------------
#if defined(__GNUC__)
__attribute__ ((interrupt(PORT1_VECTOR)))
#else
#pragma vector=PORT1_VECTOR
__interrupt
#endif
void Port_1(void) {
        // On vérifie que c'est bien P1.3 (CLK) qui a déclenché l'interruption
        if (P1IFG & BIT3) {
            if (current_mode == 0) {
                // *DÉBUG* : On bascule l'état de la LED (P1.0) à chaque clic d'encodeur
                P1OUT ^= BIT0; 

                // On lit l'état de la broche DT (P1.4) pour connaître le sens
                if (P1IN & BIT4) {
                    // Sens Horaire : On augmente la fréquence (on diminue la période)
                    if (current_period > 10) { // Limite haute fréquence (10 = 100 kHz max)
                        current_period -= 500;  
                    }
                } else {
                    // Sens Anti-horaire : On diminue la fréquence (on augmente la période)
                    if (current_period < 100000) { // Limite basse fréquence (100000 = 10 Hz min)
                        current_period += 500;
                    }
                }

                // Met à jour la fréquence du signal en temps réel  
                TA0CCR0 = current_period - 1;
                TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; // Maintient le rapport cyclique 

                // Obligatoire : On efface le drapeau pour dire qu'on a traité l'interruption
                P1IFG &= ~BIT3; 
            }
            else if (current_mode == 1) {
                // *DÉBUG* : On bascule l'état de la LED (P1.0) à chaque clic d'encodeur
                P1OUT ^= BIT0; 

                // On lit l'état de la broche DT (P1.4) pour connaître le sens
                if (P1IN & BIT4) {
                    // Sens Horaire : On augmente le rapport cyclique de 5%
                    if (current_duty_cycle < 95) { // Limite 95% max pour ne pas avoir un signal bloqué à 1
                        current_duty_cycle += 5;
                    }
                } else {
                    // Sens Anti-horaire : On diminue le rapport cyclique de 5%
                    if (current_duty_cycle > 5) { // Limite 5% min pour ne pas avoir un signal bloqué à 0
                        current_duty_cycle -= 5;
                    }
                }

                // Met à jour le rapport cyclique en temps réel  
                TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; 

                // Obligatoire : On efface le drapeau pour dire qu'on a traité l'interruption
                P1IFG &= ~BIT3; 
            }
        }  
        else if (P1IFG & BIT5) {
            // Si c'est le switch du rotary encoder qui a déclenché l'interruption
            current_mode = (current_mode + 1) % 2; // Alterne entre les modes 0 et 1
            P1IFG &= ~BIT5; // Efface le drapeau d'interruption
        }  
        else if (P1FG & BIT0){
            // Si c'est le bouton poussoir qui a déclenché l'interruption
            
        }
    }