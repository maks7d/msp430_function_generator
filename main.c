#include <msp430.h> // Header principal qui va inclure msp430g2553.h

// Variable globale pour stocker la période actuelle (en cycles d'horloge)
// 10000 = 100 Hz (avec une horloge de 1 MHz)
volatile unsigned int current_period = 10000;
volatile unsigned int current_duty_cycle = 50; // En pourcentage, ex: 50 pour 50%
volatile unsigned int current_rotary_mode = 0; // 0 = frequence, 1 = rapport cyclique
volatile unsigned int current_function_mode = 0; // 0 = carré, 1 = sinus, 2 = triangle

// --- LUT (Look-Up Table) pour le signal Sinusoïdal ---
// 32 échantillons représentant un cycle complet de sinus (en pourcentage de 0 à 100%)
const unsigned int sine_table[32] = {
    50, 60, 69, 78, 85, 92, 96, 99, 
    100, 99, 96, 92, 85, 78, 69, 60, 
    50, 40, 31, 22, 15, 8, 4, 1, 
    0, 1, 4, 8, 15, 22, 31, 40
};
volatile unsigned int wave_index = 0; // Position actuelle dans la table

void setup(void) {

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
    
    // Configuration de la LED RGB 
    P2DIR |= (BIT3 | BIT4 | BIT5); // P2.3, P2.4, P2.5 en sortie pour la LED RGB
    P2OUT &= ~(BIT3 | BIT4 | BIT5); // Éteindre la LED au démarrage
    
    // *DÉBUG* : Configurer la broche P1.0 en ENTRÉE pour le bouton poussoir
    P1DIR &= ~BIT0; // Entrée
    P1REN |= BIT0;  // Activer résistance interne
    P1OUT |= BIT0;  // Pull-up
    
    // Configuration des interruptions pour lire l'encodeur (sur CLK = P1.3)
    P1IE |= BIT3;            // Activer l'interruption sur P1.3
    P1IES |= BIT3;           // Déclencher sur le front descendant (quand le signal passe de 1 à 0)
    P1IFG &= ~BIT3;          // Effacer le drapeau d'interruption au cas où
    
    P1IE |= BIT5;            // Activer l'interruption sur P1.5 (switch du rotary encoder)
    P1IES |= BIT5;           // Déclencher sur le front descendant 
    P1IFG &= ~BIT5;          // Effacer le drapeau d'interruption au cas où
    
    //Configuration interruption pour le bouton poussoir (P1.0)
    P1IE |= BIT0;            // Activer l'interruption sur P1.0
    P1IES |= BIT0;           // Déclencher sur le front descendant (quand le bouton est pressé)
    P1IFG &= ~BIT0;          // Effacer le drapeau d'interruption au cas où
    
    // 5. Configuration de la base de temps du Timer A0
    TA0CTL = TASSEL_2 + MC_1; 
}

int update_frequency(){
    
        // On lit l'état de la broche DT (P1.4) pour connaître le sens
        if (P1IN & BIT4) {
            // Sens Horaire : On augmente la fréquence (diminue la période)
            unsigned int step = 500;
            // Limite haute fixée à 1 kHz (soit une période minimum de 1000)
            if (current_period > 1000) { 
                if (current_period - step < 1000) {
                    current_period = 1000; // On plafonne exactement à 1 kHz
                } else {
                    current_period -= step;  
                }
            }
        } else {
            // Sens Anti-horaire : On diminue la fréquence (augmente la période)
            unsigned int step = 500;
            if (current_period <= (65535 - step)) { // 65535 = Max d'un 16 bits (~15.2 Hz)
                current_period += step;
            }
        }
        return current_period;
};

int update_duty_cycle(){

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
    return current_duty_cycle;
}

int main(void) {
    
    setup(); // Appelle la fonction de configuration

    // Définition initiale de la fréquence et rapport cyclique (100 Hz, 50%)
    TA0CCR0 = current_period - 1; 
    TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; 
    TA0CCTL1 = OUTMOD_7; 
    
    // Activer l'interruption locale du Timer A0 (sur CCR0) pour mettre à jour les signaux dynamiques
    TA0CCTL0 = CCIE;

    // Activer les interruptions globales
    __enable_interrupt();

    // Économie d'énergie maximale
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
        switch(current_rotary_mode) {
            case 0:
                current_period = update_frequency();
                // Met à jour la fréquence du signal en temps réel  
                TA0CCR0 = current_period - 1;
                TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; // Maintient le rapport cyclique 
                break;

            case 1:
                current_duty_cycle = update_duty_cycle();
                // Met à jour le rapport cyclique en temps réel  
                TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100; 
                break; 
        }
        
        // INDISPENSABLE : Effacer le drapeau d'interruption de P1.3 !
        // Sans ça, le microcontrôleur reste bloqué dans une boucle infinie.
        P1IFG &= ~BIT3; 
    }
        
    else if (P1IFG & BIT5) {
        // Si c'est le switch du rotary encoder qui a déclenché l'interruption
        current_rotary_mode = (current_rotary_mode + 1) % 2; // Alterne entre les modes 0 et 1
        P1IFG &= ~BIT5; // Efface le drapeau d'interruption
    }  
    else if (P1IFG & BIT0){
        // Si c'est le bouton poussoir qui a déclenché l'interruption
        current_function_mode = (current_function_mode + 1) % 3; // Alterne entre les modes 0, 1 et 2
        switch(current_function_mode){
            case 0:
                P2OUT &= ~BIT3;
                P2OUT &= ~BIT4;
                P2OUT |= BIT5;
                current_function_mode = 0;
                // Retour au rapport cyclique fixe pour le mode carré
                TA0CCR1 = ((unsigned long)current_period * current_duty_cycle) / 100;
                break;

            case 1:
                P2OUT |= BIT3;
                P2OUT &= ~BIT4;
                P2OUT &= ~BIT5;
                current_function_mode = 1;
                wave_index = 0; // Réinitialise le sinus à 0 quand on bascule de mode
                break;
            
            case 2:
                P2OUT &= ~BIT3;
                P2OUT |= BIT4;
                P2OUT &= ~BIT5;
                current_function_mode = 2;
                break;                 
        }
            
        P1IFG &= ~BIT0; // Efface le drapeau d'interruption
    }
}

// ---------------------------------------------------------
// ROUTINE D'INTERRUPTION DU TIMER A0
// Modifie le rapport cyclique à la volée pour simuler
// un signal analogique complexe (Sinus) via SPWM
// ---------------------------------------------------------
#if defined(__GNUC__)
__attribute__ ((interrupt(TIMER0_A0_VECTOR)))
#else
#pragma vector=TIMER0_A0_VECTOR
__interrupt
#endif
void Timer_A0_ISR(void) {
    if (current_function_mode == 1) { // Mode Sinus
        // On met à jour le rapport cyclique avec la table des sinus
        TA0CCR1 = ((unsigned long)current_period * sine_table[wave_index]) / 100;
        wave_index = (wave_index + 1) % 32; // Boucle de 0 à 31 (taille de la table)
    }
    // Note : En mode Carré (0), on ne fait rien dans l'interruption (Duty Cycle fixe) !
}