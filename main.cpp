/*
 Ce programme permet la résolution en méthode exacte du problème d'allocation de spot publicitaires pour plusieurs marques.
 Il utilise la méthode d'epsilon-contrainte ainsi que le solveur CPLEX.
 Il a été relaxé afin de faciliter son test ainsi que son implémentation.
 
 Globalement le programme fonctionne tel que :
    - Définition des données necéssaires
    - Récupération des données JSON
    - Remplissage du modèle CPLEX
    - Exécution du problème mono-objectif de chaque objectif du problème initial
    - Résolution du problème sous epsilon-contrainte jusqu'à avoir toutes les valeurs d'epsilon (condition d'arrêt : chaque epsilon à atteint la solution extrême de l'objectif lié)
    - Expression des résultats obtenus
 
 Auteur : Romuald DURET
 */


#include <iostream>
#include <cstdlib>
#include <fstream>
#include<string.h>
#include "json.hpp"
#include <list>
#include <iterator>
#include <ilcplex/ilocplex.h>
ILOSTLBEGIN


// JSON Parser
using json = nlohmann::json;
using namespace std;


// Function that return 1 if j1 and j2 are competitive brands
int fp(string brand_type[], int j1, int j2){
    if(brand_type[j1].compare(brand_type[j2]) == 0){
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{

    // Récupération des données JSON des spots
    ifstream bks("/Users/romu/Desktop/Projets/Stage2022/CPLEX_Test/CPLEX_Test/break.json");
    json breaks = json::parse(bks);
    
    // Récupération des données JSON des marques
    ifstream bds("/Users/romu/Desktop/Projets/Stage2022/CPLEX_Test/CPLEX_Test/brands.json");
    json brands = json::parse(bds);
    
    
    // Solutions extrêmes des problèmes mono -> valeur d'arrêt des epsilon-contraintes
    float max_E2, max_E3, max_E4;
    
    // Valeurs epsilon
    float E2,E3,E4;
    
    int i, j; // index de comm_break et brand
    
    // DONNEES RECUPEREES
    
    int nb_Com_Break = (int) breaks.size() ; // m
    int nb_Brands = (int) brands.size(); // n
    
    cout << "Nombre de spots : " << nb_Com_Break << endl;
    cout << "NOmbre de marques : " << nb_Brands << endl;
    
    int prime_break[nb_Com_Break]; // fp(i)    V
    string brand_type[nb_Brands]; // fc(j1,J2)    X -> no type on the data
    
    
    float break_time[nb_Com_Break]; // T_i     V
    float brand_time[nb_Brands]; // t_j        V
    float grp_cap[nb_Brands]; // GRP_j         V
    float budget_cap[nb_Brands]; // BUDGET _j  V
    float prime[nb_Brands]; // PRIME _j        V
    float priority[nb_Brands]; // PRIORITY _j  X -> no need to take care
    
    float grp[nb_Com_Break][nb_Brands]; // grp_ij  V
    int cost_matrix[nb_Com_Break][nb_Brands]; // c_ij

    // AUTRES DONNEES
    
    int tmp_x_ij[nb_Com_Break][nb_Brands];
    
    
    // Liste des valeurs epsilon pour le revenu TV
    list<float> values;
    
    // Liste des solutions obtenues
    
    
    
    // REMPLISSAGE DES DONNEES
    for (const auto& item : breaks.items()){
        
        int cpt = stoi(item.key());

        // fill the break_time
        break_time[cpt] = item.value()["remaining_time"];
        
        
        // fill the prime_break
        string TMP_prime = item.value()["prime"];
        prime_break[cpt] = stoi(TMP_prime);
        //cout << prime_break[cpt] << endl;
        
        for (const auto& brand : brands.items()){
            int cpt_bd = stoi(brand.key());
            
            // cost
            cost_matrix[cpt][cpt_bd] = item.value()["slots"]["normal_1"]["price"];
            

            // grp_ij
            json cpt_grp = item.value()["grp"];
            for (const auto& g : cpt_grp.items()){
                if( g.key().compare(brand.value()["audience"]) == 0){
                    grp[cpt][cpt_bd] = g.value();
                }
            }
            
        }
    }
    
    
    for (const auto& brand : brands.items()){
        int cpt = stoi(brand.key());
        
        // Brand_type
        brand_type[cpt] = brand.value()["type"];
        // GRP_j
        grp_cap[cpt] = brand.value()["cost_grp"];
        
        // Brand time
        brand_time[cpt] = brand.value()["format"];
        
        // BUDGET_j
        budget_cap[cpt] = brand.value()["budget"];
        
        // PRIME_j
        prime[cpt] = brand.value()["ratio_prime"];
    }
    
    
    try
    {
        
        // DEFINITION DU MODELE
        IloEnv env;
    
        
        // LES DONNEES UTILISEES DANS LE MODELE
        IloNum Model_nb_Com_Break;
        IloNum Model_nb_Brands;
        
        
        IloNumArray Model_T_i(env, nb_Com_Break);
        IloNumArray Model_t_j(env, nb_Brands);
        IloNumArray Model_GRP_j(env, nb_Brands);
        IloNumArray Model_BUDGET_j(env, nb_Brands);
        
        
        IloArray<IloNumArray> Model_grp_ij(env, nb_Com_Break);
        IloArray<IloNumArray> Model_cost_matrix(env, nb_Com_Break);
        
        // REMPLISSAGE DU MODELE
    
        // Constantes
        Model_nb_Com_Break = nb_Com_Break;
        Model_nb_Brands = nb_Brands;
        
        
        // Donnees a propos de i
        for (i = 0; i < nb_Com_Break; i++){
            Model_T_i[i] = break_time[i];
            Model_grp_ij[i] = IloNumArray(env, nb_Brands);
            Model_cost_matrix[i] = IloNumArray(env, nb_Brands);
        
            for(j = 0; j < nb_Brands; j++){
                Model_grp_ij[i][j] = grp[i][j];
                
                Model_cost_matrix[i][j] = cost_matrix[i][j];
            }
        }
        
        // Donnees a propos de j
        for(j = 0; j < nb_Brands; j++){
            Model_t_j[j] = brand_time[j];
            Model_GRP_j[j] = grp_cap[j];
            Model_BUDGET_j[j] = budget_cap[j];
        }
        
        
        // VARIABLES : x_ij
        IloArray<IloNumVarArray> Model_Var_x_ij(env, nb_Com_Break);
        for (i = 0; i < nb_Com_Break; i++){
            Model_Var_x_ij[i] = IloNumVarArray(env, nb_Brands, 0, 1, ILOBOOL);
        }
        
        
        /* #######################
        I - Resolution mono-objectifs on note leur solution epsilon
        ####################### */
        
        // MONO-OBJECTIF TV
        
        cout <<  "Mono-objectif TV" << endl;
        
        IloModel modelTV(env);
    
        // Contraintes
        
        // Ne pas depasser le budget de chaque marque
        for (j = 0; i < nb_Brands; j++){
            IloExpr Ctr0Expr(env);
            for (i = 0; i < nb_Com_Break; i++){
                Ctr0Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
            }
            modelTV.add(Ctr0Expr <= Model_BUDGET_j[j]);
        }
        
        
        // Ne pas dépasser la limite de temps de chaque ecran
        for (i = 0; i < nb_Com_Break; i++){
            IloExpr Ctr1Expr(env);
            
            for (j = 0; j < nb_Brands; j++){
                Ctr1Expr += Model_Var_x_ij[i][j] * Model_t_j[j];
            }
            modelTV.add(Ctr1Expr <= Model_T_i[i]);
        }
       
        
        
         
         // Ne pas avoir de marques compéttitives sur le même écran
         for (i = 0; i < nb_Com_Break; i++){
             for (int j1 = 0; j1 < nb_Brands; j1++){
                 for (int j2 = 0; j2 < nb_Brands; j2++){
                     if(j1 != j2){
                         IloExpr Ctr2Expr(env);
                         Ctr2Expr += Model_Var_x_ij[i][j1] * Model_Var_x_ij[i][j2] * fp(brand_type, j1, j2);
                         modelTV.add(Ctr2Expr <= 0);
                     }
                 }
             }
         }
        
        
        // Fonction objectif
        IloExpr objTV(env);
        for (i = 0; i < nb_Com_Break; i++){
            for (j = 0; j < nb_Brands; j++){
                objTV += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
            }
        }
        modelTV.add(IloMaximize(env, objTV));
        objTV.end();
        
        // Resolution
        IloCplex monoTV(env);
        monoTV.extract(modelTV);
        monoTV.setParam(IloCplex::Threads, 1);
        monoTV.setParam(IloCplex::SimDisplay, 1);
        monoTV.setParam(IloCplex::TiLim, 3600);
         
        monoTV.exportModel("/Users/romu/Desktop/Projets/Stage2022/modelTV.lp");
        
        if (!monoTV.solve()) {
            env.error() << "Echec ... Non Lineaire?" << endl;
            throw(-1);
        }
        
        monoTV.out() << "Solution status: " << monoTV.getStatus() << endl;
        if (monoTV.getStatus() == IloAlgorithm::Unbounded)
            monoTV.out() << "F.O. non born�e." << endl;
        else
        {
            if (monoTV.getStatus() == IloAlgorithm::Infeasible)
                monoTV.out() << "Non-realisable." << endl;
            else
            {
                if (monoTV.getStatus() != IloAlgorithm::Optimal)
                    monoTV.out() << "Solution Optimale." << endl;
                else
                    monoTV.out() << "Solution realisable." << endl;

                monoTV.out() << " Valeur de la F.O. : " << (float)(monoTV.getObjValue()) << endl;
                
                max_E2 = (float)(monoTV.getObjValue());
                
                for (i = 0; i < nb_Com_Break; i++)
                {
                    monoTV.out() << " Ecran publicitaire " << i << " : " << endl;
                    for (j = 0; j < nb_Brands; j++)
                    {
                        monoTV.out() << " \t Brand num " << j << " : ";
                        monoTV.out() << monoTV.getValue(Model_Var_x_ij[i][j]) << " " ;
                        monoTV.out() << endl;
                    }
                    monoTV.out() << endl;
                }
            }
        }
        
        cout << endl;
        cout << "###############################" << endl;
        cout << endl;
        
        
        // MONO-OBJECTIF GRP
        
        cout <<  "Mono-objectif GRP" << endl;
        
        IloModel modelGRP(env);
    
        // Contraintes
        
        // Avoir un revenu des chaines TV non nul -> sinon : solution inutile
        IloExpr Ctr9Expr(env);
        for (i = 0; i < nb_Com_Break; i++){
            for (j = 0; j < nb_Brands; j++){
                Ctr9Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
            }
        }
        modelGRP.add(Ctr9Expr > 0);
        
        // Ne pas depasser le budget de chaque marque
        for (j = 0; i < nb_Brands; j++){
            IloExpr Ctr0Expr(env);
            for (i = 0; i < nb_Com_Break; i++){
                Ctr0Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
            }
            modelGRP.add(Ctr0Expr <= Model_BUDGET_j[j]);
        }
        
        
        // Ne pas dépasser la limite de temps de chaque ecran
        for (i = 0; i < nb_Com_Break; i++){
            IloExpr Ctr1Expr(env);
            
            for (j = 0; j < nb_Brands; j++){
                Ctr1Expr += Model_Var_x_ij[i][j] * Model_t_j[j];
            }
            modelGRP.add(Ctr1Expr <= Model_T_i[i]);
        }
         
         // Ne pas avoir de marques compéttitives sur le même écran
         for (i = 0; i < nb_Com_Break; i++){
             for (int j1 = 0; j1 < nb_Brands; j1++){
                 for (int j2 = 0; j2 < nb_Brands; j2++){
                     if(j1 != j2){
                         IloExpr Ctr2Expr(env);
                         Ctr2Expr += Model_Var_x_ij[i][j1] * Model_Var_x_ij[i][j2] * fp(brand_type, j1, j2);
                         modelTV.add(Ctr2Expr <= 0);
                     }
                 }
             }
         }
        
        // Fonction objectif
        
        IloExpr objGRP(env);
        for (i = 0; i < nb_Com_Break; i++){
            for (j = 0; j < nb_Brands; j++){
                objGRP += Model_Var_x_ij[i][j] * Model_grp_ij[i][j];
            }
        }
        modelGRP.add(IloMaximize(env, objGRP));
        objGRP.end();
        
        
        // Resolution
        IloCplex monoGRP(env);
        monoGRP.extract(modelGRP);
        monoGRP.setParam(IloCplex::Threads, 1);
        monoGRP.setParam(IloCplex::SimDisplay, 1);
        monoGRP.setParam(IloCplex::TiLim, 3600);
         
        monoGRP.exportModel("/Users/romu/Desktop/Projets/Stage2022/modelGRP.lp");
        
        if (!monoGRP.solve()) {
            env.error() << "Echec ... Non Lineaire?" << endl;
            throw(-1);
        }
        
        monoGRP.out() << "Solution status: " << monoGRP.getStatus() << endl;
        if (monoGRP.getStatus() == IloAlgorithm::Unbounded)
            monoGRP.out() << "F.O. non born�e." << endl;
        else
        {
            if (monoGRP.getStatus() == IloAlgorithm::Infeasible)
                monoGRP.out() << "Non-realisable." << endl;
            else
            {
                if (monoGRP.getStatus() != IloAlgorithm::Optimal)
                    monoGRP.out() << "Solution Optimale." << endl;
                else
                    monoGRP.out() << "Solution realisable." << endl;

                monoGRP.out() << " Valeur de la F.O. : " << (float)(monoGRP.getObjValue()) << endl;
                
                for (i = 0; i < nb_Com_Break; i++)
                {
                    monoGRP.out() << " Ecran publicitaire " << i << " : " << endl;
                    
                    for (j = 0; j < nb_Brands; j++)
                    {
                        monoGRP.out() << " \t Brand num " << j << " : ";
                        monoGRP.out() << monoGRP.getValue(Model_Var_x_ij[i][j]) << " " ;
                        monoGRP.out() << endl;
                    }
                    monoGRP.out() << endl;
                }
            }
        }
        
        float test = 0.0;
        for (i = 0; i < nb_Com_Break; i++){
            for (j = 0; j < nb_Brands; j++){
                test += monoGRP.getValue(Model_Var_x_ij[i][j]) * Model_cost_matrix[i][j] * Model_t_j[j];
            }
        }
        E2 = test;
        cout << "E2 = " << E2 << endl;
        
        values.push_back(E2);
      
        cout << endl;
        cout << "###############################" << endl;
        cout << endl;
        
        /* #######################
        II - Boucler sur le problème de base jusqu'à obtenir toutes les solutions en variant les E-contraintes
        ####################### */
        
        cout <<  "RESOLUTION NORMALE" << endl;
        
        while (E2 != max_E2){
            
            IloModel model(env);
            
            //
            IloExpr Ctr4Expr(env);
            for (i = 0; i < nb_Com_Break; i++){
                for (j = 0; j < nb_Brands; j++){
                    Ctr4Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
                }
            }
            model.add(Ctr4Expr > E2);
            
            
            // Ne pas depasser le budget de chaque marque
            for (j = 0; i < nb_Brands; j++){
                
                IloExpr Ctr0Expr(env);
                for (i = 0; i < nb_Com_Break; i++){
                    Ctr0Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * Model_t_j[j];
                }
                
                model.add(Ctr0Expr <= Model_BUDGET_j[j]);
            }
            
            
            
            // Ne pas dépasser la limite de temps de chaque ecran
            for (i = 0; i < nb_Com_Break; i++){
                IloExpr Ctr1Expr(env);
                
                for (j = 0; j < nb_Brands; j++){
                    Ctr1Expr += Model_Var_x_ij[i][j] * Model_t_j[j];
                }
                model.add(Ctr1Expr <= Model_T_i[i]);
            }
            
             // Ne pas avoir de marques compéttitives sur le même écran
             for (i = 0; i < nb_Com_Break; i++){
                 for (int j1 = 0; j1 < nb_Brands; j1++){
                     for (int j2 = 0; j2 < nb_Brands; j2++){
                         if(j1 != j2){
                             IloExpr Ctr2Expr(env);
                             Ctr2Expr += Model_Var_x_ij[i][j1] * Model_Var_x_ij[i][j2] * fp(brand_type, j1, j2);
                             modelTV.add(Ctr2Expr <= 0);
                         }
                     }
                 }
             }
            
            // GRP
            IloExpr obj(env);
            for (i = 0; i < nb_Com_Break; i++){
                for (j = 0; j < nb_Brands; j++){
                    obj += Model_Var_x_ij[i][j] * Model_grp_ij[i][j];
                }
            }
            model.add(IloMaximize(env, obj));
            obj.end();
            
            
            
            // PARAMETRAGE DU SOLVEUR
            IloCplex cplex(env);
            cplex.extract(model);
            cplex.setParam(IloCplex::Threads, 1);
            cplex.setParam(IloCplex::SimDisplay, 1);
            cplex.setParam(IloCplex::TiLim, 3600);
             
            cplex.exportModel("/Users/romu/Desktop/Projets/Stage2022/model.lp");
            
            // RESOLUTION
            if (!cplex.solve()) {
                env.error() << "Echec ... Non Lineaire?" << endl;
                throw(-1);
            }
        
            
            cplex.out() << "Solution status: " << cplex.getStatus() << endl;
            if (cplex.getStatus() == IloAlgorithm::Unbounded)
                cplex.out() << "F.O. non born�e." << endl;
            else
            {
                if (cplex.getStatus() == IloAlgorithm::Infeasible)
                    cplex.out() << "Non-realisable." << endl;
                else
                {
                    if (cplex.getStatus() != IloAlgorithm::Optimal)
                        cplex.out() << "Solution Optimale." << endl;
                    else
                        cplex.out() << "Solution realisable." << endl;

                    cplex.out() << " Valeur de la F.O. : " << (float)(cplex.getObjValue()) << endl;
                    for (i = 0; i < nb_Com_Break; i++)
                    {
                        cplex.out() << " Ecran publicitaire " << i << " : " << endl;
                        for (j = 0; j < nb_Brands; j++)
                        {
                            cplex.out() << " \t Brand num " << j << " : ";
                            cplex.out() << cplex.getValue(Model_Var_x_ij[i][j]) << " " ;
                            cplex.out() << endl;
                        }
                        cplex.out() << endl;
                    }
                }
            }

            
            cplex.out() << "-> Valeur de la F.O (GRP) : " << (float)(cplex.getObjValue()) << endl;
            
            float test2 = 0.0;
            for (i = 0; i < nb_Com_Break; i++){
                for (j = 0; j < nb_Brands; j++){
                    test2 += cplex.getValue(Model_Var_x_ij[i][j]) * Model_cost_matrix[i][j] * Model_t_j[j];
                }
            }
            
            E2 = test2;
            
            values.push_back(E2);
            
            cout << "E2 = " << E2 << endl;
            
        }
        cout << endl << endl << "############################" << endl;
        
        cout << "Résultats maximisation revenus TV " << endl;
        
        // Print des resultats des revenus TV obtenus.
        auto it = values.begin();
        cout << 0 << " : "<< *it << endl;
        
        for (int k =1 ; k < values.size(); k++)
        {
            std::advance(it, 1);
            cout << k << " : "<< *it << endl;
        }
        
        cout << endl << "max E2 : " << max_E2 << endl;
        
        env.end();

        
        
        /*
        // EPSILON-CONTRAINTES
         
        // Prime
        IloExpr Ctr3Expr(env);
        for (j = 0; i < nb_Brands; j++){
            for (i = 0; i < nb_Com_Break; i++){
                Ctr3Expr += Model_Var_x_ij[i][j] * Model_cost_matrix[i][j] * prime_break[i];
            }
            
            Ctr3Expr -= prime[j];
           
            Ctr3Expr = IloAbs(Ctr3Expr);
            
        }
        model.add(Ctr3Expr <= E2);
        
        
        // Maximiser l'allocation de spots en fonction de PRIORITY
        IloExpr Ctr5Expr(env);
        for (i = 0; i < nb_Com_Break; i++){
            for (j = 0; i < nb_Brands; j++){
                Ctr5Expr += Model_Var_x_ij[i][j] * priority[j];
            }
        }
        model.add(Ctr5Expr >= E4);
         */
        
        
        
    }
    catch (IloException& e)
    {
        cerr << " ERROR: " << e << endl;
    }
    catch (...)
    {
        cerr << " ERROR" << endl;
    }
    system("PAUSE");
    
    return 0;
}



